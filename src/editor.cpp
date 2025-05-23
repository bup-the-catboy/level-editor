#include <SDL2/SDL.h>

#include <SDL2/SDL_render.h>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "portable-file-dialogs.h"
#include "imgui/imgui.h"
#include "writer.h"
#include "reader.h"

#ifdef WINDOWS
#define BINARY "b"
#else
#define BINARY
#endif

float actual_camx = 12;
float actual_camy = 8;
float camx = 0;
float camy = 0;
int windoww = 1280;
int windowh = 720;

bool quit = false;

enum EditMode {
    EDITMODE_LAYER,
    EDITMODE_CAMBOUND
};

enum Tool {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_SELECTION,
    TOOL_HAND,
    TOOL_PICKER,
};

enum RenderMode {
    RENDERMODE_OPAQUE      = 255,
    RENDERMODE_TRANSLUCENT = 100,
    RENDERMODE_INVISIBLE   = 0
};

enum WindowFlag {
    WINDOWFLAG_NONE            =      0,
    WINDOWFLAG_LEVEL_SETTINGS  = 1 << 0,
    WINDOWFLAG_LAYER_SETTINGS  = 1 << 1,
    WINDOWFLAG_TILE_PALETTE    = 1 << 2,
    WINDOWFLAG_ENTITY_PALETTE  = 1 << 3,
    WINDOWFLAG_ENTITY_SETTINGS = 1 << 4,
    WINDOWFLAG_WARPS           = 1 << 5,
};

enum LayerType {
    LAYERTYPE_TILEMAP,
    LAYERTYPE_ENTITY
};

union Point {
    struct {
        int32_t x, y;
    };
    uint64_t id;
};

union EntityProperty {
    int asInt;
    bool asBool;
    float asFloat;
    void* asPtr;
};

enum EntityPropertyType {
    EntityPropertyType_Int,
    EntityPropertyType_Bool,
    EntityPropertyType_Float,
    EntityPropertyType_String,
};

struct EntityMeta {
    const char* name;
    float width;
    float height;
    bool hidden;
    const char* texture_path;
    int cropx, cropy, cropw, croph;
    std::map<std::string, std::pair<int, union EntityProperty>> properties;
};

struct Entity {
    struct EntityMeta* meta;
    float x;
    float y;
    std::map<std::string, union EntityProperty> properties;
};

struct Layer {
    enum LayerType type;
    float smx, smy;
    float sox, soy;
    float scx, scy;
    struct Layer* entity_tilemap_layer;
    std::string name;
    std::map<uint64_t, int> tilemap;
    std::vector<struct Entity*> entities;
    int tileset;
};

struct TilesetData {
    int palette;
    const char* texture_path;
    int width, height;
    int tiles_in_row;
};

struct Warp {
    int next_theme;
    int next_music;
    int next_cambound;
    int next_level;
    int next_layer;
    float next_pos_x;
    float next_pos_y;
};

#define NO_VSCODE

#define TILE_PALETTE(_1, _2) ,TILE_PALETTE_##_1
#define ENUM(value) = x
enum TilePaletteIDs {
    __TILE_PALETTE = -1
#include "../../src/game/data/tiles.h"
};

#undef ENUM
#define TILESET_TYPE(t) int t;
#define ENUM(value)

struct ThemeDataFields {
#include "../../src/game/data/tileset_types.h"
};

union ThemeData {
    struct ThemeDataFields data;
    int arr[sizeof(struct ThemeDataFields) / sizeof(int)];
};

#define TILESET(_1, _2) { _2 },
#define SIZE(w, h) .width = w, .height = h,
#define TILES_IN_ROW(x) .tiles_in_row = x,
#define TEXTURE(x) .texture_path = x,
#define PALETTE(x) .palette = TILE_PALETTE_##x,

struct TilesetData tileset_data[] = {
#include "../../src/game/data/tilesets.h"
};

#undef TILESET
#undef ENUM
#define TILESET(_1, _2) ,TILESET_##_1
#define ENUM(value) = value

enum TilesetNames {
    __TILESET = -1
#include "../../src/game/data/tilesets.h"
};

#define THEME(_1, _2) ,THEME_##_1

enum ThemeNames {
    __THEME = -1
#include "../../src/game/data/themes.h"
};

#undef ENUM
#undef THEME
#define THEME(_1, _2) { .data = { _2 } },
#define THEME_TILESET(_1, _2) ._1 = TILESET_##_2,
#define ENUM(value)

union ThemeData theme_data[] = {
#include "../../src/game/data/themes.h"
};

#undef TEXTURE
#define ENTITY(id, data) { .name = #id, data },
#define TEXTURE(_)
#define UPDATE(_)
#define COLLISION(_)
#define FLAGS(_)
#define PRIORITY(_)
#define DEFAULT_PROPERTY(key, type, value)
#define LVLEDIT_TEXTURE(path) .texture_path = path,
#define LVLEDIT_HIDE() .hidden = true,
#define LVLEDIT_PROPERTIES(...) .properties = { __VA_ARGS__ },
#define LVLEDIT_CROP(x, y, w, h) .cropx = x, .cropy = y, .cropw = w, .croph = h,

#define int(   x, def) { #x, { EntityPropertyType_Int,    { .asInt   =        def } } },
#define bool(  x, def) { #x, { EntityPropertyType_Bool,   { .asBool  =        def } } },
#define float( x, def) { #x, { EntityPropertyType_Float,  { .asFloat =        def } } },
#define string(x, def) { #x, { EntityPropertyType_String, { .asPtr   = (void*)def } } },

struct EntityMeta entity_data[] = {
#include "../../src/game/data/entities.h"
};

#undef int
#undef bool
#undef float
#undef string

#undef ENUM
#undef TILE_PALETTE
#define TILE_PALETTE(id, data) , __TILE_##id = -1 data
#define TILE(id, _1)           ,   TILE_##id
#define ENUM(value) = value
enum TileIDs {
    __TILE = -1
#include "../../src/game/data/tiles.h"
};

#undef TEXTURE
#undef TILE
#undef LVLEDIT_HIDE
#undef TILE_PALETTE
#define TILE_PALETTE(id, data) curr_palette = TILE_PALETTE_##id; data
#define TILE(id, data) curr_tile = TILE_##id; data
#define TEXTURE(_1)
#define SOLID()
#define SIMPLE_STATIONARY_TEXTURE(_1) SIMPLE_ANIMATED_TEXTURE(1, 1, _1)
#define LVLEDIT_HIDE()
#define SIMPLE_ANIMATED_TEXTURE(frames, delay, ...) tile_frame_data[curr_palette][curr_tile] = { true, delay, { __VA_ARGS__ } };
struct {
    bool textured;
    int delay;
    std::vector<int> frames;
} tile_frame_data[256][256];

void init_tile_frame_data() {
    int curr_palette = 0;
    int curr_tile = 0;
#include "../../src/game/data/tiles.h"
}

#define curr_tileset tileset_data[theme_data[curr_theme].arr[curr_layer->tileset]]
#define tile_texture_data tile_frame_data[curr_tileset.palette]

#undef THEME
#define THEME(_1, _2) #_1 "\0"

    const char* str_music =
        "grass" "\0"
    ;
    const char* str_theme =
#include "../../src/game/data/themes.h"
    ;

#undef THEME
#undef THEME_TILESET
#define THEME(_1, _2) [THEME_##_1] = _2,
#define THEME_TILESET(_1, _2) #_1 " (" #_2 ")" "\0"

const char* strs_theme[256] = {
#include "../../src/game/data/themes.h"
};

#define str_tileset strs_theme[curr_theme]

struct Rectangle {
    int x1, y1, x2, y2;
};

extern SDL_Renderer* renderer;
extern SDL_Window* window;

enum EditMode curr_mode = EDITMODE_LAYER;
enum Tool curr_tool = TOOL_BRUSH;
enum Tool selected_tool = TOOL_BRUSH;
enum RenderMode curr_rendermode = RENDERMODE_TRANSLUCENT;
int shown_windows = WINDOWFLAG_NONE;
bool lock_to_grid = false;
bool show_grid = true;
bool show_screen = false;
bool lock_screen_to_grid = false;

int curr_music = 0;
int curr_theme = 0;
int curr_cambound = 0;
struct Layer* curr_layer = NULL;
std::vector<std::vector<Rectangle>> cambounds = {};
std::vector<struct Layer> layers = {};
std::vector<struct Warp> warps = {};
std::map<std::string, SDL_Texture*> texture_cache = {};
int selected_tile = 0;
struct Entity* grabbed_entity;
struct Entity* current_entity;
struct EntityMeta* selected_entity;
struct {
    int *x, *y;
} grabbed_point;

int frames_drawn = 0;
int layers_created = 0;

std::string format_string(const char* fmt, ...) {
    char out[2048];
    va_list list;
    va_start(list, fmt);
    vsnprintf(out, 2048, fmt, list);
    va_end(list);
    return out;
}

SDL_Texture* get_texture(const char* texture_path) {
    if (!texture_path) return NULL;
    if (texture_cache.find(texture_path) == texture_cache.end()) {
        int w, h, c;
        unsigned char* data = stbi_load((std::string("../assets/") + texture_path).c_str(), &w, &h, &c, 4);
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(data, w, h, 32, 4 * w, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        texture_cache.insert({ texture_path, texture });
    }
    return texture_cache[texture_path];
}

Rectangle fix_rectangle(Rectangle rect) {
    Rectangle fixed = rect;
    if (fixed.x2 < fixed.x1) std::swap(fixed.x1, fixed.x2);
    if (fixed.y2 < fixed.y1) std::swap(fixed.y1, fixed.y2);
    return fixed;
}

void set_tile(int x, int y, int tile) {
    uint64_t coord = (Point){ .x = x, .y = y }.id;
    auto pos = curr_layer->tilemap.find(coord);
    if (tile == 0) {
        if (pos != curr_layer->tilemap.end()) curr_layer->tilemap.erase(pos);
    }
    else {
        if (pos == curr_layer->tilemap.end()) curr_layer->tilemap.insert({ coord, tile });
        else curr_layer->tilemap[coord] = tile;
    }
}

int get_tile_from_layer(struct Layer* layer, int x, int y) {
    uint64_t coord = (Point){ .x = x, .y = y }.id;
    auto pos = layer->tilemap.find(coord);
    if (pos == layer->tilemap.end()) return 0;
    return layer->tilemap[coord];
}

int get_tile(int x, int y) {
    return get_tile_from_layer(curr_layer, x, y);
}

void draw_grid(SDL_Renderer* renderer, float scalex, float scaley, float speedx, float speedy, float offsetx, float offsety, float tile_width, float tile_height) {
    float grid_width  = tile_width  * 2.f;
    float grid_height = tile_height * 2.f;
    float camX = (camx / grid_width  * 32) / scalex * speedx + offsetx;
    float camY = (camy / grid_height * 32) / scaley * speedy + offsety;
    int width = windoww / scalex / grid_width + 4;
    int height = windowh / scaley / grid_height + 4;
    int fromX = camX - width / 2.f;
    int fromY = camY - height / 2.f;
    int toX = camX + width / 2.f;
    int toY = camY + height / 2.f;
    for (int y = fromY; y <= toY; y++) {
        for (int x = fromX; x <= toX; x++) {
            SDL_Rect rect = (SDL_Rect){
                .x = (int)((x - camX) * grid_width  * scalex + windoww / 2.f),
                .y = (int)((y - camY) * grid_height * scaley + windowh / 2.f),
                .w = (int)(grid_width  * scalex) + 1,
                .h = (int)(grid_height * scaley) + 1
            };
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

void draw_tilemap(struct Layer* layer) {
    struct Layer* prev_curr_layer = curr_layer;
    curr_layer = layer;
    float grid_width  = curr_tileset.width  * 2.f;
    float grid_height = curr_tileset.height * 2.f;
    float camX = ((camx * layer->smx - windoww / 64.f) / grid_width  * 32) / layer->scx + layer->sox;
    float camY = ((camy * layer->smy - windowh / 64.f) / grid_height * 32) / layer->scy + layer->soy;
    int fromX = camX - 1;
    int fromY = camY - 1;
    int toX = camX + windoww / layer->scx / grid_width + 1;
    int toY = camY + windowh / layer->scy / grid_height + 1;
    for (int y = fromY; y <= toY; y++) {
        for (int x = fromX; x <= toX; x++) {
            int tile = get_tile_from_layer(layer, x, y);
            if (!tile_texture_data[tile].textured) continue;
            int anim = tile_texture_data[tile].frames[(frames_drawn / tile_texture_data[tile].delay) % tile_texture_data[tile].frames.size()];
            SDL_Texture* tex = get_texture(curr_tileset.texture_path);
            SDL_Rect src = (SDL_Rect){
                .x = (anim % curr_tileset.tiles_in_row) * curr_tileset.width,
                .y = (anim / curr_tileset.tiles_in_row) * curr_tileset.height,
                .w = curr_tileset.width,
                .h = curr_tileset.height
            };
            SDL_Rect dst = (SDL_Rect){
                .x = (int)((x - camX) * grid_width  * layer->scx),
                .y = (int)((y - camY) * grid_height * layer->scy),
                .w = (int)(grid_width  * layer->scx),
                .h = (int)(grid_height * layer->scy)
            };
            Uint8 a;
            SDL_GetRenderDrawColor(renderer, NULL, NULL, NULL, &a);
            SDL_SetTextureAlphaMod(tex, a);
            SDL_RenderCopy(renderer, tex, &src, &dst);
            SDL_SetTextureAlphaMod(tex, 255);
        }
    }
    curr_layer = prev_curr_layer;
}

void draw_entities(struct Layer* layer) {
    struct Layer* values = layer->entity_tilemap_layer ? layer->entity_tilemap_layer : layer;
    struct Layer* prev_curr_layer = curr_layer;
    curr_layer = layer;
    float grid_width  = curr_tileset.width  * 2.f;
    float grid_height = curr_tileset.height * 2.f;
    float camX = ((camx * values->smx - windoww / 64.f) / grid_width  * 32) / values->scx + values->sox;
    float camY = ((camy * values->smy - windowh / 64.f) / grid_height * 32) / values->scy + values->soy;
    for (struct Entity* entity : layer->entities) {
        SDL_Texture* tex = get_texture(entity->meta->texture_path);
        int width, height;
        SDL_QueryTexture(tex, NULL, NULL, &width, &height);
        if (
            entity->meta->cropx != 0 ||
            entity->meta->cropy != 0 ||
            entity->meta->cropw != 0 ||
            entity->meta->croph != 0
        ) {
            width = entity->meta->cropw;
            height = entity->meta->croph;
        }
        SDL_Rect dst = (SDL_Rect){
            .x = (int)((entity->x - camX) * grid_width  * values->scx) - width,
            .y = (int)((entity->y - camY) * grid_height * values->scy) - height * 2,
            .w = width  * 2,
            .h = height * 2
        };
        SDL_Rect src = (SDL_Rect){
            entity->meta->cropx, entity->meta->cropy,
            entity->meta->cropw, entity->meta->croph
        };
        Uint8 a;
        SDL_GetRenderDrawColor(renderer, NULL, NULL, NULL, &a);
        SDL_SetTextureAlphaMod(tex, a);
        SDL_RenderCopy(renderer, tex, &src, &dst);
        SDL_SetTextureAlphaMod(tex, 255);
    }
    curr_layer = prev_curr_layer;
}

void add_layer(enum LayerType type) {
    struct Layer layer;
    layer.scx = layer.scy = 1;
    layer.smx = layer.smy = 1;
    layer.sox = layer.soy = 0;
    layer.type = type;
    layer.entity_tilemap_layer = NULL;
    layer.name = "Layer " + std::to_string(++layers_created);
    layer.tileset = 0;
    int pos = 0;
    for (int i = 0; i < layers.size(); i++) {
        if (&layers[i] == curr_layer) {
            pos = i;
            break;
        }
    }
    auto iter = layers.insert(layers.begin() + pos, layer);
    curr_layer = &*iter; // i loove c++
}

void delete_layer(int index) {
    struct Layer* layer = &layers[index];
    for (struct Layer l : layers) {
        if (l.entity_tilemap_layer == layer) l.entity_tilemap_layer = NULL;
    }
    std::string id = "";
    if (curr_layer) id = curr_layer->name;
    layers.erase(layers.begin() + index);
    if (id != "") {
        curr_layer = layers.size() == 0 ? NULL : &layers[index == layers.size() ? index - 1 : index];
        for (int i = 0; i < layers.size(); i++) {
            if (layers[i].name == id) curr_layer = &layers[i];
        }
    }
}

void move_layer(int from, int to) {
    std::string id = "";
    if (curr_layer) id = curr_layer->name;
    struct Layer temp = layers[from];
    layers[from] = layers[to];
    layers[to] = temp;
    if (id != "") {
        for (int i = 0; i < layers.size(); i++) {
            if (layers[i].name == id) curr_layer = &layers[i];
        }
    }
}

#define _
#define CTRL  c = ctrl;
#define SHIFT s = shift;
#define ALT   a = alt;
#define MODIF(modif) ({ \
    bool c = !ctrl;      \
    bool s = !shift;      \
    bool a = !alt;         \
    modif                   \
    c && s && a;             \
})
#define TOGGLE_WINDOW(flag) (shown_windows = (shown_windows & (flag) ? (shown_windows & ~(flag)) : (shown_windows | (flag))))

void get_position_from_pixel(int inx, int iny, float* outx, float* outy) {
    if (!curr_layer) return;
    struct Layer* values = curr_layer->entity_tilemap_layer ? curr_layer->entity_tilemap_layer : curr_layer;
    float grid_width  = curr_tileset.width  * 2.f * values->scx;
    float grid_height = curr_tileset.height * 2.f * values->scy;
    if (outx) *outx = (inx / grid_width ) + (((camx * values->smx - windoww / 64.f) / (curr_tileset.width  * 2) * 32) / values->scx) + values->sox;
    if (outy) *outy = (iny / grid_height) + (((camy * values->smy - windowh / 64.f) / (curr_tileset.height * 2) * 32) / values->scy) + values->soy;
}

void get_tile_position_from_pixel(int inx, int iny, int* outx, int* outy) {
    float f_outx, f_outy;
    get_position_from_pixel(inx, iny, &f_outx, &f_outy);
    if (outx) *outx = floor(f_outx);
    if (outy) *outy = floor(f_outy);
}

struct Entity* create_entity(float x, float y) {
    if (!selected_entity) return NULL;
    struct Entity* entity = new struct Entity();
    entity->x = x;
    entity->y = y;
    entity->meta = selected_entity;
    entity->properties = {};
    for (auto prop : entity->meta->properties) {
        entity->properties.insert({ prop.first, prop.second.second });
        if (prop.second.first == EntityPropertyType_String) {
            union EntityProperty& e = entity->properties[prop.first];
            e.asPtr = calloc(256, 1);
            strcpy((char*)e.asPtr, (char*)prop.second.second.asPtr);
        }
    }
    return entity;
}

void stub_start(int x, int y) {}
void stub_drag(int x, int y, int dx, int dy) {}
void stub_end() {}

void tile_modif_drag(int x, int y, int tile) {
    int tileX, tileY;
    get_tile_position_from_pixel(x, y, &tileX, &tileY);
    set_tile(tileX, tileY, tile);
}

void set_grabbed_entity_position(int x, int y) {
    if (!grabbed_entity) return;
    get_position_from_pixel(x, y, &grabbed_entity->x, &grabbed_entity->y);
    grabbed_entity->y += grabbed_entity->meta->height / 2;
    if (lock_to_grid) grabbed_entity->x = round(grabbed_entity->x * 4) / 4.f;
    if (lock_to_grid) grabbed_entity->y = round(grabbed_entity->y * 4) / 4.f;
}

bool is_on_entity(struct Entity* entity, int x, int y) {
    float px, py;
    get_position_from_pixel(x, y, &px, &py);
    return
        px > entity->x - entity->meta->width / 2 &&
        px < entity->x + entity->meta->width / 2 &&
        py > entity->y - entity->meta->height    &&
        py < entity->y;
}

void brush_drag(int x, int y, int dx, int dy) {
    if (!curr_layer) return;
    if (curr_layer->type == LAYERTYPE_TILEMAP) tile_modif_drag(x, y, selected_tile);
    if (curr_layer->type == LAYERTYPE_ENTITY) set_grabbed_entity_position(x, y);
}

void eraser_drag(int x, int y, int dx, int dy) {
    if (!curr_layer) return;
    if (curr_layer->type == LAYERTYPE_TILEMAP) tile_modif_drag(x, y, 0);
    if (curr_layer->type == LAYERTYPE_ENTITY) {
        for (int i = 0; i < curr_layer->entities.size(); i++) {
            struct Entity* entity = curr_layer->entities[i];
            if (is_on_entity(entity, x, y)) {
                for (auto& prop : entity->properties) {
                    if (entity->meta->properties[prop.first].first == EntityPropertyType_String) free(prop.second.asPtr);
                }
                delete entity;
                curr_layer->entities.erase(curr_layer->entities.begin() + i);
                i--;
            }
        }
    }
}

void brush_start(int x, int y) {
    if (!curr_layer) return;
    if (curr_layer->type == LAYERTYPE_ENTITY) {
        float px, py;
        get_position_from_pixel(x, y, &px, &py);
        struct Entity* entity = create_entity(px, py);
        if (entity) {
            entity->meta = selected_entity;
            grabbed_entity = current_entity = entity;
            curr_layer->entities.push_back(entity);
        }
    }
    brush_drag(x, y, 0, 0);
}

void eraser_start(int x, int y) {
    eraser_drag(x, y, 0, 0);
}

void hand_start(int x, int y) {
    for (struct Entity* entity : curr_layer->entities) {
        if (is_on_entity(entity, x, y)) grabbed_entity = current_entity = entity;
    }
}

void hand_drag(int x, int y, int dx, int dy) {
    set_grabbed_entity_position(x, y);
}

void brush_end() {
    grabbed_entity = NULL;
}

void picker_start(int x, int y) {
    if (!curr_layer) return;
    if (curr_layer->type == LAYERTYPE_TILEMAP) {
        int tx, ty;
        get_tile_position_from_pixel(x, y, &tx, &ty);
        selected_tile = get_tile(tx, ty);
    }
    if (curr_layer->type == LAYERTYPE_ENTITY) {
        for (struct Entity* entity : curr_layer->entities) {
            if (is_on_entity(entity, x, y)) selected_entity = entity->meta;
        }
    }
}

struct {
    void(*start)(int x, int y);
    void(*drag)(int x, int y, int dx, int dy);
    void(*end)();
    char letter;
} tools[] = {
    { brush_start, brush_drag, brush_end, 'B' },
    { eraser_start, eraser_drag, stub_end, 'E' },
    { stub_start, stub_drag, stub_end, 'S' },
    { hand_start, hand_drag, brush_end, 'H' },
    { picker_start, stub_drag, stub_end, 'P' },
};

void get_tilemap_size(struct Layer* layer, int* tilemap_width, int* tilemap_height, int* offset_x, int* offset_y) {
    int w = 0, h = 0, ox = 0, oy = 0;
    if (!layer->tilemap.empty()) {
        int left  = INT32_MAX;
        int right = INT32_MIN;
        int up    = INT32_MAX;
        int down  = INT32_MIN;
        for (auto tile : layer->tilemap) {
            union Point p = *(union Point*)&tile.first;
            int x = p.x;
            int y = p.y;
            if (left  > x) left  = x;
            if (right < x) right = x;
            if (up    > y) up    = y;
            if (down  < y) down  = y;
        }
        w = right - left + 1;
        h = down - up + 1;
        ox = left;
        oy = up;
    }
    if (tilemap_width)  *tilemap_width  = w;
    if (tilemap_height) *tilemap_height = h;
    if (offset_x)       *offset_x       = ox;
    if (offset_y)       *offset_y       = oy;
}

char* create_tilemap_data(struct Layer* layer, int* width, int* height, int* offset_x, int* offset_y) {
    int w, h, ox, oy;
    get_tilemap_size(layer, &w, &h, &ox, &oy);
    char* data = (char*)malloc(w * h);
    memset(data, 0, w * h);
    for (auto tile : layer->tilemap) {
        union Point p = *(union Point*)&tile.first;
        data[(p.y - oy) * w + (p.x - ox)] = tile.second;
    }
    if (width)    *width  = w;
    if (height)   *height = h;
    if (offset_x) *offset_x = ox;
    if (offset_y) *offset_y = oy;
    return data;
}

void parse_level(unsigned char* data) {
    ReadStream* stream = reader_create(data);

    reader_goto(stream);
    curr_theme = reader_read<int32_t>(stream);
    curr_music = reader_read<int32_t>(stream);
    curr_cambound = reader_read<int32_t>(stream);
    reader_pop_block(stream);

    reader_goto(stream);
    int num_cambounds = reader_read<int32_t>(stream);
    cambounds.clear();
    for (int i = 0; i < num_cambounds; i++) {
        reader_goto(stream);
        int num_rects = reader_read<int32_t>(stream);
        cambounds.push_back({});
        for (int j = 0; j < num_rects; j++) {
            Rectangle rect;
            rect.x1 = reader_read<float>(stream);
            rect.y1 = reader_read<float>(stream);
            rect.x2 = reader_read<float>(stream) + rect.x1;
            rect.y2 = reader_read<float>(stream) + rect.y1;
            cambounds.back().push_back(rect);
        }
        reader_pop_block(stream);
    }
    reader_pop_block(stream);

    reader_goto(stream);
    int num_warps = reader_read<int32_t>(stream);
    for (int i = 0; i < num_warps; i++) {
        reader_goto(stream);
        Warp warp;
        warp.next_theme    = reader_read<int32_t>(stream);
        warp.next_music    = reader_read<int32_t>(stream);
        warp.next_cambound = reader_read<int32_t>(stream);
        warp.next_level    = reader_read<int32_t>(stream);
        warp.next_layer    = reader_read<int32_t>(stream);
        warp.next_pos_x    = reader_read<float  >(stream);
        warp.next_pos_y    = reader_read<float  >(stream);
        warps.push_back(warp);
        reader_pop_block(stream);
    }
    reader_pop_block(stream);

    reader_goto(stream);
    layers = {};
    curr_layer = NULL;
    layers_created = 0;
    int num_layers = reader_read<int32_t>(stream);
    struct EntityMeta* selected_entity_prev = selected_entity;
    for (int i = 0; i < num_layers; i++) {
        reader_goto(stream);
        int type = reader_read<int32_t>(stream);
        add_layer((enum LayerType)type);
        curr_layer->smx = reader_read<float>(stream);
        curr_layer->smy = reader_read<float>(stream);
        curr_layer->sox = reader_read<float>(stream);
        curr_layer->soy = reader_read<float>(stream);
        curr_layer->scx = reader_read<float>(stream);
        curr_layer->scy = reader_read<float>(stream);
        reader_goto(stream);

        switch ((enum LayerType)type) {
        case LAYERTYPE_TILEMAP: {
            curr_layer->tileset = reader_read<int32_t>(stream);
            int width = reader_read<int32_t>(stream);
            int height = reader_read<int32_t>(stream);
            for (int j = 0; j < width * height; j++) {
                uint8_t tile = reader_read<uint8_t>(stream);
                int x = j % width;
                int y = j / width;
                set_tile(x, y, tile);
            }
        } break;

        case LAYERTYPE_ENTITY: {
            int tilemap_index = reader_read<int32_t>(stream);
            int num_entities = reader_read<int32_t>(stream);
            curr_layer->entity_tilemap_layer = (Layer*)(uintptr_t)tilemap_index;
            for (int j = 0; j < num_entities; j++) {
                reader_goto(stream);
                uint8_t id = reader_read<uint8_t>(stream);
                reader_skip(stream, 3);
                float x = reader_read<float>(stream);
                float y = reader_read<float>(stream);
                if (!entity_data[id].hidden) {
                    selected_entity = &entity_data[id];
                    struct Entity* entity = create_entity(x, y);
                    curr_layer->entities.push_back(entity);
                    int num_properties = reader_read<int32_t>(stream);
                    for (int k = 0; k < num_properties; k++) {
                        reader_goto(stream);
                        std::string name = reader_read_string(stream);
                        int type = reader_read<uint8_t>(stream);
                        switch (type) {
                            case EntityPropertyType_Int:   entity->properties[name].asInt   = reader_read<int32_t>(stream); break;
                            case EntityPropertyType_Bool:  entity->properties[name].asBool  = reader_read<uint8_t>(stream); break;
                            case EntityPropertyType_Float: entity->properties[name].asFloat = reader_read<float  >(stream); break;
                            case EntityPropertyType_String:
                                entity->properties[name].asPtr = calloc(sizeof(char), 256);
                                std::strncpy((char*)entity->properties[name].asPtr, reader_read_string(stream).c_str(), 256);
                                break;
                        }
                        reader_pop_block(stream);
                    }
                }
                reader_pop_block(stream);
            }
        } break;
        }

        reader_pop_block(stream);
        reader_pop_block(stream);
    }
    std::reverse(layers.begin(), layers.end());
    selected_entity = selected_entity_prev;
    curr_layer = NULL;
    actual_camx = 12;
    actual_camy = 8;
    reader_pop_block(stream);

    reader_close(stream);

    for (int i = 0; i < layers.size(); i++) {
        if (layers[i].type != LAYERTYPE_ENTITY) continue;
        if ((uintptr_t)layers[i].entity_tilemap_layer == (uintptr_t)-1)
            layers[i].entity_tilemap_layer = NULL;
        else
            layers[i].entity_tilemap_layer = &layers[(uintptr_t)layers[i].entity_tilemap_layer];
    }
}

void new_level() {
    parse_level((unsigned char[]){
        0x10,0x00,0x00,0x00,0x1C,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00
    });
}

std::string last_saved = "";

void open_file() {
    std::vector<std::string> file = pfd::open_file("Open Level", "", { "Level Files", "*.lvl", "All Files", "*" }, pfd::opt::none).result();
    if (file.empty()) return;
    last_saved = file[0];
    struct stat s;
    stat(file[0].c_str(), &s);
    FILE* f = fopen(file[0].c_str(), "r" BINARY);
    unsigned char* data = (unsigned char*)malloc(s.st_size);
    fread(data, s.st_size, 1, f);
    fclose(f);
    parse_level(data);
    free(data);
}

#define arrindex(arr, ptr) (((uintptr_t)(ptr) - (uintptr_t)(arr)) / sizeof(*(arr)))
void save_file(bool force_select) {
    if (last_saved.empty() || force_select) {
        last_saved = pfd::save_file("Save Level", "", { "Level Files", "*.lvl", "All Files", "*" }, pfd::opt::none).result();
        if (last_saved.empty()) return;
    }

    WriteStream* stream = writer_create(16);
    writer_make_offset(stream, 12);
    writer_write<int32_t>(stream, curr_theme);
    writer_write<int32_t>(stream, curr_music);
    writer_write<int32_t>(stream, curr_cambound);
    writer_pop_block(stream);

    writer_make_offset(stream, cambounds.size() * 4 + 4);
    writer_write<int32_t>(stream, cambounds.size());
    for (auto cambound : cambounds) {
        writer_make_offset(stream, cambound.size() * 16 + 4);
        writer_write<int32_t>(stream, cambound.size());
        for (auto rect : cambound) {
            rect = fix_rectangle(rect);
            writer_write<float>(stream, rect.x1);
            writer_write<float>(stream, rect.y1);
            writer_write<float>(stream, rect.x2 - rect.x1);
            writer_write<float>(stream, rect.y2 - rect.y1);
        }
        writer_pop_block(stream);
    }
    writer_pop_block(stream);

    writer_make_offset(stream, 4 + warps.size() * 4);
    writer_write<int32_t>(stream, warps.size());
    for (struct Warp warp : warps) {
        writer_make_offset(stream, 4 * 7);
        writer_write<int32_t>(stream, warp.next_theme);
        writer_write<int32_t>(stream, warp.next_music);
        writer_write<int32_t>(stream, warp.next_cambound);
        writer_write<int32_t>(stream, warp.next_level);
        writer_write<int32_t>(stream, warp.next_layer);
        writer_write<float>  (stream, warp.next_pos_y);
        writer_write<float>  (stream, warp.next_pos_x);
        writer_pop_block(stream);
    }
    writer_pop_block(stream);

    writer_make_offset(stream, 4 + layers.size() * 4);
    writer_write<int32_t>(stream, layers.size());
    for (struct Layer layer : layers) {
        Layer* settings_layer = &layer;
        int width, height;
        char* tilemap_data = NULL;
        int tilemap_index = -1;
        int offset_x = 0, offset_y = 0;
        if (layer.type == LAYERTYPE_ENTITY && layer.entity_tilemap_layer) {
            settings_layer = layer.entity_tilemap_layer;
            get_tilemap_size(settings_layer, &width, &height, &offset_x, &offset_y);
        }
        if (layer.type == LAYERTYPE_TILEMAP) tilemap_data = create_tilemap_data(&layer, &width, &height, &offset_x, &offset_y);
        writer_make_offset(stream, 32);
        writer_write<int32_t>(stream, layer.type);
        writer_write<float>(stream, settings_layer->smx);
        writer_write<float>(stream, settings_layer->smy);
        writer_write<float>(stream, settings_layer->sox - offset_x);
        writer_write<float>(stream, settings_layer->soy - offset_y);
        writer_write<float>(stream, settings_layer->scx);
        writer_write<float>(stream, settings_layer->scy);

        switch (layer.type) {
            case LAYERTYPE_TILEMAP:
            writer_make_offset(stream, 12 + width * height);
            writer_write<int32_t>(stream, layer.tileset);
            writer_write<int32_t>(stream, width);
            writer_write<int32_t>(stream, height);
            for (int i = 0; i < width * height; i++) {
                writer_write<uint8_t>(stream, tilemap_data[i]);
            }
            writer_pop_block(stream);
            break;

            case LAYERTYPE_ENTITY:
            for (int i = 0; i < layers.size(); i++) {
                if (&layers[i] == layer.entity_tilemap_layer) {
                    tilemap_index = i;
                    break;
                }
            }
            writer_make_offset(stream, 8 + layer.entities.size() * 4);
            writer_write<int32_t>(stream, tilemap_index);
            writer_write<int32_t>(stream, layer.entities.size());
            for (int i = 0; i < layer.entities.size(); i++) {
                writer_make_offset(stream, 16 + layer.entities[i]->properties.size() * 4);
                writer_write<uint8_t>(stream, arrindex(entity_data, layer.entities[i]->meta));
                writer_skip(stream, 3);
                writer_write<float>(stream, layer.entities[i]->x - offset_x);
                writer_write<float>(stream, layer.entities[i]->y - offset_y);
                writer_write<int32_t>(stream, layer.entities[i]->properties.size());
                for (auto& prop : layer.entities[i]->properties) {
                    int type = layer.entities[i]->meta->properties[prop.first].first;
                    writer_make_offset(stream, 2 + prop.first.length() + (
                        type == EntityPropertyType_Int    ? 4 :
                        type == EntityPropertyType_Bool   ? 1 :
                        type == EntityPropertyType_Float  ? 4 :
                        type == EntityPropertyType_String ? strlen((char*)prop.second.asPtr) + 1: 0
                    ));
                    writer_write_ptr(stream, (void*)prop.first.c_str(), prop.first.length() + 1);
                    writer_write<uint8_t>(stream, type);
                    switch (type) {
                        case EntityPropertyType_Int:    writer_write<int32_t>(stream, prop.second.asInt);   break;
                        case EntityPropertyType_Bool:   writer_write<uint8_t>(stream, prop.second.asBool);  break;
                        case EntityPropertyType_Float:  writer_write<float  >(stream, prop.second.asFloat); break;
                        case EntityPropertyType_String: writer_write_ptr     (stream, prop.second.asPtr, strlen((char*)prop.second.asPtr) + 1); break;
                    }
                    writer_pop_block(stream);
                }
                writer_pop_block(stream);
            }
            writer_pop_block(stream);
            break;
        }

        if (tilemap_data) free(tilemap_data);

        writer_pop_block(stream);
    }

    int size;
    char* data = writer_close(stream, &size);
    FILE* f = fopen(last_saved.c_str(), "w" BINARY);
    fwrite(data, size, 1, f);
    fclose(f);
    free(data);
}

void activate_shortcut(bool ctrl, bool shift, bool alt, char letter) {
    if (MODIF(CTRL)) {
        switch (letter) {
            case 'N': new_level(); break;
            case 'O': open_file(); break;
            case 'S': save_file(false); break;
            case 'Q': quit = true; break;
            case 'L': curr_mode = EDITMODE_LAYER; break;
            case 'B': curr_mode = EDITMODE_CAMBOUND; break;
            case 'Z': break; // todo
            case 'C': break; // todo
            case 'V': break; // todo
            case 'X': break; // todo
            case 'D': break; // todo
            case 'T': add_layer(LAYERTYPE_TILEMAP); break;
            case 'E': add_layer(LAYERTYPE_ENTITY); break;
            case 'R': actual_camx = 12; actual_camy = 8; break;
        }
    }
    if (MODIF(CTRL SHIFT)) {
        switch (letter) {
            case 'S': save_file(true); break;
            case 'Z': break; // todo
            case 'O': curr_rendermode = RENDERMODE_OPAQUE; break;
            case 'T': curr_rendermode = RENDERMODE_TRANSLUCENT; break;
            case 'I': curr_rendermode = RENDERMODE_INVISIBLE; break;
            case 'L': lock_to_grid ^= 1; break;
        }
    }
    if (MODIF(CTRL ALT)) {
        switch (letter) {
            case 'S': TOGGLE_WINDOW(WINDOWFLAG_LEVEL_SETTINGS); break;
            case 'L': TOGGLE_WINDOW(WINDOWFLAG_LAYER_SETTINGS); break;
            case 'T': TOGGLE_WINDOW(WINDOWFLAG_TILE_PALETTE); break;
            case 'E': TOGGLE_WINDOW(WINDOWFLAG_ENTITY_PALETTE); break;
            case 'P': TOGGLE_WINDOW(WINDOWFLAG_ENTITY_SETTINGS); break;
            case 'W': TOGGLE_WINDOW(WINDOWFLAG_WARPS); break;
        }
    }
    if (MODIF(ALT)) {
        switch (letter) {
            case 'G': show_grid ^= 1; break;
            case 'S': show_screen ^= 1; break;
            case 'L': lock_screen_to_grid ^= 1; break;
        }
    }
    if (MODIF(_)) {
        switch (letter) {
            case 'B': selected_tool = TOOL_BRUSH; break;
            case 'E': selected_tool = TOOL_ERASER; break;
            case 'S': selected_tool = TOOL_SELECTION; break;
            case 'H': selected_tool = TOOL_HAND; break;
            case 'P': selected_tool = TOOL_PICKER; break;
        }
    }
}

bool should_quit() {
    return quit;
}

#undef CTRL
#undef SHIFT
#undef ALT
#undef _
#define _              false, false, false,
#define CTRL           true,  false, false,
#define SHIFT          false, true,  false,
#define ALT            false, false, true,
#define CTRL_SHIFT     true,  true,  false,
#define CTRL_ALT       true,  false, true,
#define CTRL_SHIFT_ALT true,  true,  true,
#define SHIFT_ALT      false, true,  true,

#define WINDOW(flag, title) if (({       \
    bool run = shown_windows & (flag);    \
    if (run) {                             \
        bool shown = true;                  \
        ImGui::Begin(title, &shown);         \
        if (!shown) shown_windows &= ~(flag); \
    }                                          \
    run;                                        \
}))

bool inited = false;
bool init() {
    if (inited) return true;
    inited = true;
    init_tile_frame_data();
    return false;
}

void editor_run(SDL_Renderer* renderer) {
    init();
    SDL_GetWindowSize(window, &windoww, &windowh);

    camx = actual_camx;
    camy = actual_camy;
    if (lock_screen_to_grid) {
        camx = round(camx);
        camy = round(camy);
    }

    if (ImGui::BeginMainMenuBar()) {
        ImGui::BeginDisabled();
        ImGui::Text("(%c)", tools[selected_tool].letter);
        ImGui::EndDisabled();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) activate_shortcut(CTRL 'N');
            if (ImGui::MenuItem("Open", "Ctrl+O")) activate_shortcut(CTRL 'O');
            if (ImGui::MenuItem("Save", "Ctrl+S")) activate_shortcut(CTRL 'S');
            if (ImGui::MenuItem("Save As", "Ctrl+Shift+S")) activate_shortcut(CTRL_SHIFT 'S');
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Ctrl+Q")) activate_shortcut(CTRL 'Q');
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::SeparatorText("Modes");
            if (ImGui::MenuItem("Layer Mode", "Ctrl+L", curr_mode == EDITMODE_LAYER)) activate_shortcut(CTRL 'L');
            if (ImGui::MenuItem("Camera Bound Mode", "Ctrl+B", curr_mode == EDITMODE_CAMBOUND)) activate_shortcut(CTRL 'B');
            ImGui::SeparatorText("Actions");
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) activate_shortcut(CTRL 'Z');
            if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z")) activate_shortcut(CTRL_SHIFT 'Z');
            if (ImGui::MenuItem("Copy", "Ctrl+C")) activate_shortcut(CTRL 'C');
            if (ImGui::MenuItem("Paste", "Ctrl+V")) activate_shortcut(CTRL 'V');
            if (ImGui::MenuItem("Cut", "Ctrl+X")) activate_shortcut(CTRL 'X');
            if (ImGui::MenuItem("Delete", "Ctrl+D")) activate_shortcut(CTRL 'D');
            if (ImGui::MenuItem("Reset Camera", "Ctrl+R")) activate_shortcut(CTRL 'R');
            ImGui::SeparatorText("Tools");
            if (ImGui::MenuItem("Brush", "B", selected_tool == TOOL_BRUSH)) activate_shortcut(_ 'B');
            if (ImGui::MenuItem("Eraser", "E", selected_tool == TOOL_ERASER)) activate_shortcut(_ 'E');
            if (ImGui::MenuItem("Selection", "S", selected_tool == TOOL_SELECTION)) activate_shortcut(_ 'S');
            if (ImGui::MenuItem("Hand", "H", selected_tool == TOOL_HAND)) activate_shortcut(_ 'H');
            if (ImGui::MenuItem("Picker", "P", selected_tool == TOOL_PICKER)) activate_shortcut(_ 'P');
            ImGui::SeparatorText("Settings");
            if (ImGui::MenuItem("Lock Entities to Grid", "Ctrl+Shift+L", lock_to_grid)) activate_shortcut(CTRL_SHIFT 'L');
            if (ImGui::MenuItem("Lock Screen to Grid", "Alt+L", lock_screen_to_grid)) activate_shortcut(ALT 'L');
            ImGui::SeparatorText("Layers");
            if (ImGui::BeginMenu("Add Layer")) {
                if (ImGui::MenuItem("Tile Layer", "Ctrl+T")) activate_shortcut(CTRL 'T');
                if (ImGui::MenuItem("Entity Layer", "Ctrl+E")) activate_shortcut(CTRL 'E');
                ImGui::EndMenu();
            }
            for (int i = 0; i < layers.size(); i++) {
                if (ImGui::SmallButton(("X##" + std::to_string(i)).c_str())) delete_layer(i); ImGui::SameLine();
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton(("^##" + std::to_string(i)).c_str())) move_layer(i, i - 1); ImGui::SameLine();
                ImGui::EndDisabled();
                ImGui::BeginDisabled(i == layers.size() - 1);
                if (ImGui::SmallButton(("v##" + std::to_string(i)).c_str())) move_layer(i, i + 1); ImGui::SameLine();
                ImGui::EndDisabled();
                ImGui::TextDisabled("(%c)", "TE"[layers[i].type]);
                ImGui::SameLine();
                if (ImGui::MenuItem(layers[i].name.c_str(), NULL, curr_layer == &layers[i])) curr_layer = &layers[i];
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Render Other Layers As")) {
                if (ImGui::MenuItem("Opaque", "Ctrl+Shift+O", curr_rendermode == RENDERMODE_OPAQUE)) activate_shortcut(CTRL_SHIFT 'O');
                if (ImGui::MenuItem("Translucent", "Ctrl+Shift+T", curr_rendermode == RENDERMODE_TRANSLUCENT)) activate_shortcut(CTRL_SHIFT 'T');
                if (ImGui::MenuItem("Invisible", "Ctrl+Shift+I", curr_rendermode == RENDERMODE_INVISIBLE)) activate_shortcut(CTRL_SHIFT 'I');
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Show Grid", "Alt+G", show_grid)) activate_shortcut(ALT 'G');
            if (ImGui::MenuItem("Show Screen", "Alt+S", show_screen)) activate_shortcut(ALT 'S');
            ImGui::SeparatorText("Windows");
            if (ImGui::MenuItem("Level Settings", "Ctrl+Alt+S", shown_windows & WINDOWFLAG_LEVEL_SETTINGS)) activate_shortcut(CTRL_ALT 'S');
            if (ImGui::MenuItem("Layer Settings", "Ctrl+Alt+L", shown_windows & WINDOWFLAG_LAYER_SETTINGS)) activate_shortcut(CTRL_ALT 'L');
            if (ImGui::MenuItem("Tile Palette", "Ctrl+Alt+T", shown_windows & WINDOWFLAG_TILE_PALETTE)) activate_shortcut(CTRL_ALT 'T');
            if (ImGui::MenuItem("Entity Palette", "Ctrl+Alt+E", shown_windows & WINDOWFLAG_ENTITY_PALETTE)) activate_shortcut(CTRL_ALT 'E');
            if (ImGui::MenuItem("Entity Settings", "Ctrl+Alt+P", shown_windows & WINDOWFLAG_ENTITY_SETTINGS)) activate_shortcut(CTRL_ALT 'P');
            if (ImGui::MenuItem("Warps", "Ctrl+Alt+W", shown_windows & WINDOWFLAG_WARPS)) activate_shortcut(CTRL_ALT 'W');
            ImGui::EndMenu();
        }
        std::string text = format_string("Pos: %.2f %.2f", camx, camy);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x);
        ImGui::TextDisabled("%s", text.c_str());
        ImGui::EndMainMenuBar();
    }

    WINDOW(WINDOWFLAG_LEVEL_SETTINGS, "Level Settings") {
        ImGui::PushItemWidth(200);
        ImGui::Combo("Music", &curr_music, str_music);
        ImGui::Combo("Theme", &curr_theme, str_theme);
        ImGui::SeparatorText("Camera Bounds");
        ImGui::BeginDisabled(cambounds.size() == 0);
        ImGui::InputInt("Bound ID", &curr_cambound);
        if (ImGui::Button("Delete")) {
            cambounds.erase(cambounds.begin() + curr_cambound);
            curr_cambound--;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            curr_cambound = cambounds.size();
            cambounds.push_back({});
        }
        ImGui::PopItemWidth();
        ImGui::End();
        if (curr_cambound < 0) curr_cambound = 0;
        if (curr_cambound >= cambounds.size()) curr_cambound = cambounds.size() - 1;
    }

    WINDOW(WINDOWFLAG_LAYER_SETTINGS, "Layer Settings") {
        if (curr_layer) {
            ImGui::PushItemWidth(100);
            struct Layer* values = curr_layer->entity_tilemap_layer ? curr_layer->entity_tilemap_layer : curr_layer;
            ImGui::BeginDisabled(values != curr_layer);
            ImGui::SliderFloat("##scale", &values->scx, 0.1f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SameLine();
            ImGui::SliderFloat("Scale", &values->scy, 0.1f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("##scrlmult", &values->smx, 0.1f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SameLine();
            ImGui::SliderFloat("Scroll Speed", &values->smy, 0.1f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("##scrloff", &values->sox, -10.f, 10.f, "%.3f");
            ImGui::SameLine();
            ImGui::SliderFloat("Scroll Offset", &values->soy, -10.f, 10.f, "%.3f");
            ImGui::EndDisabled();
            ImGui::PopItemWidth();
            if (curr_layer->type == LAYERTYPE_TILEMAP) {
                ImGui::Combo("Tileset", &curr_layer->tileset, str_tileset);
            }
            if (curr_layer->type == LAYERTYPE_ENTITY) {
                if (ImGui::BeginCombo("Tilemap Layer", (
                    curr_layer->entity_tilemap_layer == NULL
                        ? std::string("(None)")
                        : curr_layer->entity_tilemap_layer->name
                ).c_str())) {
                    if (ImGui::Selectable("(None)")) curr_layer->entity_tilemap_layer = NULL;
                    for (int i = 0; i < layers.size(); i++) {
                        if (layers[i].type != LAYERTYPE_TILEMAP) continue;
                        if (ImGui::Selectable(layers[i].name.c_str())) curr_layer->entity_tilemap_layer = &layers[i];
                    }
                    ImGui::EndCombo();
                }
            }
        }
        else ImGui::Text("No layer is selected.");
        ImGui::End();
    }

    WINDOW(WINDOWFLAG_TILE_PALETTE, "Tile Palette") {
        if (curr_layer) {
            int i = 0;
            int curr_tile_id = 0;
            const char* name = "";
            bool hide = false;
            ImGui::BeginTable("##tile_picker_table", 5);
#undef TEXTURE
#undef TILE
#undef LVLEDIT_HIDE
#undef SIMPLE_ANIMATED_TEXTURE
#undef TILE_PALETTE
#define TILE_PALETTE(_1, _2) if (curr_tileset.palette == TILE_PALETTE_##_1) { _2 }
#define TILE(_1, _2) if (i % 5 == 0) ImGui::TableNextRow(); name = #_1; curr_tile_id = TILE_##_1; hide = false; _2 if (!hide) i++;
#define LVLEDIT_HIDE() hide = true;
#define SIMPLE_ANIMATED_TEXTURE(_1, delay, ...) if (!hide) {                                            \
                int frames[] = { __VA_ARGS__ };                                                          \
                int curr_frame = frames[(frames_drawn / delay) % (sizeof(frames) / sizeof(int))];         \
                int width, height;                                                                         \
                SDL_Texture* tex = get_texture(curr_tileset.texture_path);                                  \
                SDL_QueryTexture(tex, NULL, NULL, &width, &height);                                          \
                float u1 = (curr_frame % curr_tileset.tiles_in_row) / ((float)width  / curr_tileset.width);   \
                float v1 = (curr_frame / curr_tileset.tiles_in_row) / ((float)height / curr_tileset.height);   \
                float u2 = (curr_frame % curr_tileset.tiles_in_row + 1) / ((float)width  / curr_tileset.width); \
                float v2 = (curr_frame / curr_tileset.tiles_in_row + 1) / ((float)height / curr_tileset.height); \
                ImGui::TableNextColumn();                                                                         \
                ImGui::BeginDisabled(selected_tile == curr_tile_id);                                               \
                if (ImGui::ImageButton(                                                                             \
                    ("tile" + std::to_string(i)).c_str(),                                                            \
                    tex,                                                                                              \
                    ImVec2(                                                                                            \
                        curr_tileset.width * 2,                                                                         \
                        curr_tileset.height * 2                                                                          \
                    ), ImVec2(u1, v1), ImVec2(u2, v2)                                                                     \
                )) selected_tile = curr_tile_id;                                                                           \
                ImGui::SetItemTooltip("%s", name);                                                                          \
                ImGui::EndDisabled();                                                                                        \
            }
#include "../../src/game/data/tiles.h"
            ImGui::EndTable();
        }
        else ImGui::Text("No tileset is selected");
        ImGui::End();
    }
    WINDOW(WINDOWFLAG_ENTITY_PALETTE, "Entity Palette") {
        int iter = 0;
        ImGui::BeginTable("##entity_picker_table", 5);
        for (int i = 0; i < sizeof(entity_data) / sizeof(*entity_data); i++) {
            if (entity_data[i].hidden) continue;
            if (iter % 5 == 0) ImGui::TableNextRow(0, 32.f);
            ImGui::TableNextColumn();
            SDL_Texture* tex = get_texture(entity_data[i].texture_path);
            int width, height;
            int cropx = 0, cropy = 0;
            SDL_QueryTexture(tex, NULL, NULL, &width, &height);
            float tex_width = width;
            float tex_height = height;
            if (
                entity_data[i].cropx != 0 ||
                entity_data[i].cropy != 0 ||
                entity_data[i].cropw != 0 ||
                entity_data[i].croph != 0
            ) {
                cropx = entity_data[i].cropx;
                cropy = entity_data[i].cropy;
                width = entity_data[i].cropw;
                height = entity_data[i].croph;
            }
            ImVec2 u = ImVec2(cropx / tex_width, cropy / tex_height);
            ImVec2 v = ImVec2((cropx + width) / tex_width, (cropy + height) / tex_height);
            width *= 2;
            height *= 2;
            if (width > 32) {
                height = ((float)width / height) / (width / 32.f);
                width = 32;
            }
            if (height > 32) {
                width = ((float)height / width) / (height / 32.f);
                height = 32;
            }
            ImGui::BeginDisabled(selected_entity == &entity_data[i]);
            if (ImGui::ImageButton(
                ("entity" + std::to_string(iter)).c_str(),
                tex,
                ImVec2(width, height),
                u, v
            )) selected_entity = &entity_data[i];
            ImGui::SetItemTooltip("%s", entity_data[i].name);
            ImGui::EndDisabled();
            iter++;
        }
        ImGui::EndTable();
        ImGui::End();
    }
    WINDOW(WINDOWFLAG_ENTITY_SETTINGS, "Entity Settings") {
        if (!current_entity) ImGui::Text("No entity selected");
        else {
            ImGui::DragFloat("X", &current_entity->x, 1 / (32.f * curr_layer->scx));
            ImGui::DragFloat("Y", &current_entity->y, 1 / (32.f * curr_layer->scy));
            if (current_entity->meta->properties.empty()) ImGui::Text("No properties to edit");
            else {
                for (auto prop : current_entity->meta->properties) {
                    if (prop.second.first == EntityPropertyType_Int)    ImGui::DragInt  (prop.first.c_str(),       &current_entity->properties[prop.first].asInt);
                    if (prop.second.first == EntityPropertyType_Float)  ImGui::DragFloat(prop.first.c_str(),       &current_entity->properties[prop.first].asFloat);
                    if (prop.second.first == EntityPropertyType_Bool)   ImGui::Checkbox (prop.first.c_str(),       &current_entity->properties[prop.first].asBool);
                    if (prop.second.first == EntityPropertyType_String) ImGui::InputText(prop.first.c_str(), (char*)current_entity->properties[prop.first].asPtr, 256);
                }
            }
        }
        ImGui::End();
    }
    WINDOW(WINDOWFLAG_WARPS, "Warps") {
        if (ImGui::Button("Add")) {
            warps.push_back((Warp){
                .next_theme = 0,
                .next_music = 0,
                .next_cambound = 0,
                .next_level = 0,
                .next_layer = 0,
                .next_pos_x = 0,
                .next_pos_y = 0
            });
        }
        for (int i = 0; i < warps.size(); i++) {
            if (ImGui::SmallButton(("X##" + std::to_string(i)).c_str())) {
                warps.erase(warps.begin() + i);
                i--;
                continue;
            }
            ImGui::SameLine();
            if (ImGui::BeginMenu(("Warp " + std::to_string(i)).c_str())) {
                ImGui::PushItemWidth(200);
                ImGui::Combo("Music", &warps[i].next_music, str_music);
                ImGui::Combo("Theme", &warps[i].next_theme, str_theme);
                ImGui::InputInt("Cambound", &warps[i].next_cambound);
                ImGui::InputInt("Layer", &warps[i].next_layer);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Be careful when selecting a layer,");
                    ImGui::Text("as warping to non-entity layers");
                    ImGui::Text("crashes the game.");
                    ImGui::EndTooltip();
                }
                ImGui::InputInt("Level", &warps[i].next_level);
                ImGui::InputFloat("Pos X", &warps[i].next_pos_x);
                ImGui::InputFloat("Pos Y", &warps[i].next_pos_y);
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }
        }
        ImGui::End();
    }

    for (int i = layers.size() - 1; i >= 0; i--) {
        int opacity;
        if (curr_layer == &layers[i]) opacity = 255;
        else opacity = curr_rendermode;
        if (opacity == 0) continue;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, opacity);
        if (layers[i].type == LAYERTYPE_TILEMAP) draw_tilemap(&layers[i]);
        if (layers[i].type == LAYERTYPE_ENTITY) draw_entities(&layers[i]);
    }

    if (curr_mode == EDITMODE_LAYER) {
        if (curr_layer) {
            struct Layer* layer = curr_layer->entity_tilemap_layer ? curr_layer->entity_tilemap_layer : curr_layer;
            if (layer->type != LAYERTYPE_ENTITY) {
                SDL_SetRenderDrawColor(renderer, 176, 176, 176, 255);
                if (show_grid) draw_grid(renderer, layer->scx, layer->scy, layer->smx, layer->smy, layer->sox, layer->soy, curr_tileset.width, curr_tileset.height);
            }
        }
    }
    if (curr_mode == EDITMODE_CAMBOUND) {
        SDL_SetRenderDrawColor(renderer, 176, 176, 176, 255);
        if (show_grid) draw_grid(renderer, 1, 1, 1, 1, 0, 0, 16, 16);
    }

    if (!cambounds.empty()) {
        for (Rectangle rect : cambounds[curr_cambound]) {
            rect = fix_rectangle(rect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 32);
            float x1 = (rect.x1 - (camx - windoww / 64.f)) * 32.f;
            float y1 = (rect.y1 - (camy - windowh / 64.f)) * 32.f;
            float x2 = (rect.x2 - (camx - windoww / 64.f)) * 32.f;
            float y2 = (rect.y2 - (camy - windowh / 64.f)) * 32.f;
            SDL_FRect r = (SDL_FRect){ .x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1 };
            SDL_RenderFillRectF(renderer, &r);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRectF(renderer, &r);
            if (curr_mode == EDITMODE_CAMBOUND) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                r = { .x = x1 - 4, .y = y1 - 4, .w = 8, .h = 8 }; SDL_RenderFillRectF(renderer, &r);
                r = { .x = x2 - 4, .y = y1 - 4, .w = 8, .h = 8 }; SDL_RenderFillRectF(renderer, &r);
                r = { .x = x2 - 4, .y = y2 - 4, .w = 8, .h = 8 }; SDL_RenderFillRectF(renderer, &r);
                r = { .x = x1 - 4, .y = y2 - 4, .w = 8, .h = 8 }; SDL_RenderFillRectF(renderer, &r);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    if (show_screen) {
        SDL_Rect rect = (SDL_Rect){
            .x = windoww / 2 - (24 * 32) / 2,
            .y = windowh / 2 - (16 * 32) / 2,
            .w = 24 * 32,
            .h = 16 * 32
        };
        SDL_RenderDrawRect(renderer, &rect);
    }

    frames_drawn++;
}

void editor_process_event(SDL_Event* event) {
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym >= SDLK_a && event->key.keysym.sym <= SDLK_z) activate_shortcut(
                event->key.keysym.mod & KMOD_CTRL,
                event->key.keysym.mod & KMOD_SHIFT,
                event->key.keysym.mod & KMOD_ALT,
                event->key.keysym.sym - SDLK_a + 'A'
            );
        }
    }

    if (!ImGui::GetIO().WantCaptureMouse) {
        if (event->type == SDL_MOUSEMOTION) {
            if (event->motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                actual_camx -= event->motion.xrel / 32.f;
                actual_camy -= event->motion.yrel / 32.f;
            }
        }

        if (curr_mode == EDITMODE_LAYER) {
            if (event->type == SDL_MOUSEMOTION) {
                if (event->motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    tools[curr_tool].drag(
                        event->motion.x, event->motion.y,
                        event->motion.xrel, event->motion.yrel
                    );
                }
            }
            if (event->type == SDL_MOUSEBUTTONDOWN) {
                if (event->button.button == SDL_BUTTON_LEFT) {
                    curr_tool = selected_tool;
                    tools[curr_tool].start(event->button.x, event->button.y);
                }
            }
            if (event->type == SDL_MOUSEBUTTONUP) {
                if (event->button.button == SDL_BUTTON_LEFT) tools[curr_tool].end();
            }
        }
        else if (curr_mode == EDITMODE_CAMBOUND && !cambounds.empty()) {
            if (event->type == SDL_MOUSEMOTION) {
                if (event->motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    if (grabbed_point.x && grabbed_point.y) {
                        float x = event->button.x / 32.f + (camx - windoww / 64.f) + 0.5;
                        float y = event->button.y / 32.f + (camy - windowh / 64.f) + 0.5;
                        *grabbed_point.x = floor(x);
                        *grabbed_point.y = floor(y);
                    }
                }
            }
            if (event->type == SDL_MOUSEBUTTONDOWN) {
                if (event->button.button == SDL_BUTTON_LEFT) {
                    float x = event->button.x / 32.f + (camx - windoww / 64.f) + 0.5;
                    float y = event->button.y / 32.f + (camy - windowh / 64.f) + 0.5;
                    int  tx = floor(x);
                    int  ty = floor(y);
                    float d = sqrt((x - tx - 0.5) * (x - tx - 0.5) + (y - ty - 0.5) * (y - ty - 0.5));
                    if (d <= 0.25) for (auto& rect : cambounds[curr_cambound]) {
                        if      (rect.x1 == tx && rect.y1 == ty) grabbed_point = (typeof(grabbed_point)){ .x = &rect.x1, .y = &rect.y1 };
                        else if (rect.x2 == tx && rect.y1 == ty) grabbed_point = (typeof(grabbed_point)){ .x = &rect.x2, .y = &rect.y1 };
                        else if (rect.x1 == tx && rect.y2 == ty) grabbed_point = (typeof(grabbed_point)){ .x = &rect.x1, .y = &rect.y2 };
                        else if (rect.x2 == tx && rect.y2 == ty) grabbed_point = (typeof(grabbed_point)){ .x = &rect.x2, .y = &rect.y2 };
                        else continue;
                        break;
                    }
                    if (grabbed_point.x && grabbed_point.y) return;
                    cambounds[curr_cambound].push_back({ .x1 = tx, .y1 = ty, .x2 = tx, .y2 = ty });
                    grabbed_point = (typeof(grabbed_point)){ .x = &cambounds[curr_cambound].back().x2, .y = &cambounds[curr_cambound].back().y2 };
                }
            }
            if (event->type == SDL_MOUSEBUTTONUP) {
                if (event->button.button == SDL_BUTTON_LEFT) {
                    auto& c = cambounds[curr_cambound];
                    c.erase(std::remove_if(c.begin(), c.end(), [](Rectangle& rect) {
                        return rect.x1 == rect.x2 || rect.y1 == rect.y2;
                    }), c.end());
                    grabbed_point.x = grabbed_point.y = NULL;
                }
            }
        }
    }
}