/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_view.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_main.h"

PPB_Core* g_pCore;
PPB_Fullscreen* g_pFullscreen;
PPB_Graphics2D* g_pGraphics2D;
PPB_ImageData* g_pImageData;
PPB_Instance* g_pInstance;
PPB_View* g_pView;
PPB_InputEvent* g_pInputEvent;
PPB_KeyboardInputEvent* g_pKeyboardInput;
PPB_MouseInputEvent* g_pMouseInput;
PPB_TouchInputEvent* g_pTouchInput;

struct {
  PP_Resource ctx;
  struct PP_Size size;
  int bound;
  uint8_t* cell_in;
  uint8_t* cell_out;
  int32_t cell_stride;
} g_Context;


const unsigned int kInitialRandSeed = 0xC0DE533D;
const int kCellAlignment = 0x10;

#define INLINE inline __attribute__((always_inline))

/* BGRA helper macro, for constructing a pixel for a BGRA buffer. */
#define MakeBGRA(b, g, r, a)  \
  (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

/* 128 bit vector types */
typedef uint8_t u8x16_t __attribute__ ((vector_size (16)));

/* Helper function to broadcast x across 16 element vector. */
INLINE u8x16_t broadcast(uint8_t x) {
  u8x16_t r = {x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x};
  return r;
}


/*
 * Convert a count value into a live (green) or dead color value.
 */
const uint32_t kNeighborColors[] = {
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
};

/*
 * These represent the new health value of a cell based on its neighboring
 * values.  The health is binary: either alive or dead.
 */
const uint8_t kIsAlive[] = {
      0, 0, 0, 0, 0, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0
};

void UpdateContext(uint32_t width, uint32_t height) {
  int stride = (width + kCellAlignment - 1) & ~kCellAlignment;
  if (width != g_Context.size.width || height != g_Context.size.height) {

    size_t size = stride * height;
    size_t index;

    free(g_Context.cell_in);
    free(g_Context.cell_out);

    /* Create a new context */
    void* in_buffer = NULL;
    void* out_buffer = NULL;
    /* alloc buffers aligned on 16 bytes */
    posix_memalign(&in_buffer, kCellAlignment, size);
    posix_memalign(&out_buffer, kCellAlignment, size);
    g_Context.cell_in = (uint8_t*) in_buffer;
    g_Context.cell_out = (uint8_t*) out_buffer;

    memset(g_Context.cell_out, 0, size);
    for (index = 0; index < size; index++) {
      g_Context.cell_in[index] = rand() & 1;
    }
  }

  /* Recreate the graphics context on a view change */
  g_pCore->ReleaseResource(g_Context.ctx);
  g_Context.size.width = width;
  g_Context.size.height = height;
  g_Context.cell_stride = stride;
  g_Context.ctx =
      g_pGraphics2D->Create(PSGetInstanceId(), &g_Context.size, PP_TRUE);
  g_Context.bound =
      g_pInstance->BindGraphics(PSGetInstanceId(), g_Context.ctx);
}

void DrawCell(int32_t x, int32_t y) {
  int32_t width = g_Context.size.width;
  int32_t height = g_Context.size.height;
  int32_t stride = g_Context.cell_stride;

  if (!g_Context.cell_in) return;

  if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
    g_Context.cell_in[x - 1 + y * stride] = 1;
    g_Context.cell_in[x + 1 + y * stride] = 1;
    g_Context.cell_in[x + (y - 1) * stride] = 1;
    g_Context.cell_in[x + (y + 1) * stride] = 1;
  }
}

void ProcessTouchEvent(PSEvent* event) {
  uint32_t count = g_pTouchInput->GetTouchCount(event->as_resource,
      PP_TOUCHLIST_TYPE_TOUCHES);
  uint32_t i, j;
  for (i = 0; i < count; i++) {
    struct PP_TouchPoint touch = g_pTouchInput->GetTouchByIndex(
        event->as_resource, PP_TOUCHLIST_TYPE_TOUCHES, i);
    int radius = (int)touch.radius.x;
    int x = (int)touch.position.x;
    int y = (int)touch.position.y;
    /* num = 1/100th the area of touch point */
    int num = (int)(M_PI * radius * radius / 100.0f);
    for (j = 0; j < num; j++) {
      int dx = rand() % (radius * 2) - radius;
      int dy = rand() % (radius * 2) - radius;
      /* only plot random cells within the touch area */
      if (dx * dx + dy * dy <= radius * radius)
        DrawCell(x + dx, y + dy);
    }
  }
}

void ProcessEvent(PSEvent* event) {
  switch(event->type) {
    /* If the view updates, build a new Graphics 2D Context */
    case PSE_INSTANCE_DIDCHANGEVIEW: {
      struct PP_Rect rect;

      g_pView->GetRect(event->as_resource, &rect);
      UpdateContext(rect.size.width, rect.size.height);
      break;
    }

    case PSE_INSTANCE_HANDLEINPUT: {
      PP_InputEvent_Type type = g_pInputEvent->GetType(event->as_resource);
      PP_InputEvent_Modifier modifiers =
          g_pInputEvent->GetModifiers(event->as_resource);

      switch(type) {
        case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
          struct PP_Point location =
              g_pMouseInput->GetPosition(event->as_resource);
          /* If the button is down, draw */
          if (modifiers & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
            DrawCell(location.x, location.y);
          }
          break;
        }

        case PP_INPUTEVENT_TYPE_TOUCHSTART:
        case PP_INPUTEVENT_TYPE_TOUCHMOVE:
          ProcessTouchEvent(event);
          break;

        case PP_INPUTEVENT_TYPE_KEYDOWN: {
          PP_Bool fullscreen = g_pFullscreen->IsFullscreen(PSGetInstanceId());
          g_pFullscreen->SetFullscreen(PSGetInstanceId(),
                                       fullscreen ? PP_FALSE : PP_TRUE);
          break;
        }

        default:
          break;
      }
      /* case PSE_INSTANCE_HANDLEINPUT */
      break;
    }

    default:
      break;
  }
}


void Stir() {
  int32_t width = g_Context.size.width;
  int32_t height = g_Context.size.height;
  int32_t stride = g_Context.cell_stride;
  int32_t i;
  if (g_Context.cell_in == NULL || g_Context.cell_out == NULL)
    return;

  for (i = 0; i < width; ++i) {
    g_Context.cell_in[i] = rand() & 1;
    g_Context.cell_in[i + (height - 1) * stride] = rand() & 1;
  }
  for (i = 0; i < height; ++i) {
    g_Context.cell_in[i * stride] = rand() & 1;
    g_Context.cell_in[i * stride + (width - 1)] = rand() & 1;
  }
}


void Render() {
  struct PP_Size* psize = &g_Context.size;
  PP_ImageDataFormat format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;

  /*
   * Create a buffer to draw into.  Since we are waiting until the next flush
   * chrome has an opportunity to cache this buffer see ppb_graphics_2d.h.
   */
  PP_Resource image =
      g_pImageData->Create(PSGetInstanceId(), format, psize, PP_FALSE);
  uint8_t* pixels = g_pImageData->Map(image);

  struct PP_ImageDataDesc desc;
  uint8_t* cell_temp;
  uint32_t x, y;

  /* If we somehow have not allocated these pointers yet, skip this frame. */
  if (!g_Context.cell_in || !g_Context.cell_out) return;

  /* Get the pixel stride. */
  g_pImageData->Describe(image, &desc);

  /* Stir up the edges to prevent the simulation from reaching steady state. */
  Stir();

  /*
   * Do neighbor summation; apply rules, output pixel color. Note that a 1 cell
   * wide perimeter is excluded from the simulation update; only cells from
   * x = 1 to x < width - 1 and y = 1 to y < height - 1 are updated.
   */

  for (y = 1; y < g_Context.size.height - 1; ++y) {
    uint8_t *src0 = (g_Context.cell_in + (y - 1) * g_Context.cell_stride);
    uint8_t *src1 = src0 + g_Context.cell_stride;
    uint8_t *src2 = src1 + g_Context.cell_stride;
    uint8_t *dst = (g_Context.cell_out + y * g_Context.cell_stride) + 1;
    uint32_t *pixel_line =  (uint32_t*) (pixels + y * desc.stride);
    const u8x16_t kOne = broadcast(1);
    const u8x16_t kFour = broadcast(4);
    const u8x16_t kEight = broadcast(8);
    const u8x16_t kZero255 = {0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    /* Prime the src */
    u8x16_t src00 = *(u8x16_t*)&src0[0];
    u8x16_t src01 = *(u8x16_t*)&src0[16];
    u8x16_t src10 = *(u8x16_t*)&src1[0];
    u8x16_t src11 = *(u8x16_t*)&src1[16];
    u8x16_t src20 = *(u8x16_t*)&src2[0];
    u8x16_t src21 = *(u8x16_t*)&src2[16];

    /* This inner loop is SIMD - each loop iteration will process 16 cells. */
    for (x = 1; (x + 15) < (g_Context.size.width - 1); x += 16) {

      /*
       * Construct jittered source temps, using __builtin_shufflevector(..) to
       * extract a shifted 16 element vector from the 32 element concatenation
       * of two source vectors.
       */
      u8x16_t src0j0 = src00;
      u8x16_t src0j1 = __builtin_shufflevector(src00, src01,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src0j2 = __builtin_shufflevector(src00, src01,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
      u8x16_t src1j0 = src10;
      u8x16_t src1j1 = __builtin_shufflevector(src10, src11,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src1j2 = __builtin_shufflevector(src10, src11,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
      u8x16_t src2j0 = src20;
      u8x16_t src2j1 = __builtin_shufflevector(src20, src21,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src2j2 = __builtin_shufflevector(src20, src21,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);

      /* Sum the jittered sources to construct neighbor count. */
      u8x16_t count = src0j0 + src0j1 +  src0j2 +
                      src1j0 +        +  src1j2 +
                      src2j0 + src2j1 +  src2j2;
      /* Add the center cell. */
      count = count + count + src1j1;
      /* If count > 4 and < 8, center cell will be alive in the next frame. */
      u8x16_t alive1 = count > kFour;
      u8x16_t alive2 = count < kEight;
      /* Intersect the two comparisons from above. */
      u8x16_t alive = alive1 & alive2;

      /*
       * At this point, alive[x] will be one of two values:
       *   0x00 for a dead cell
       *   0xFF for an alive cell.
       *
       * Next, convert alive cells to green pixel color.
       * Use __builtin_shufflevector(..) to construct output pixels from
       * concantination of alive vector and kZero255 const vector.
       *   Indices 0..15 select the 16 cells from alive vector.
       *   Index 16 is zero constant from kZero255 constant vector.
       *   Index 17 is 255 constant from kZero255 constant vector.
       *   Output pixel color values are in BGRABGRABGRABGRA order.
       * Since each pixel needs 4 bytes of color information, 16 cells will
       * need to expand to 4 seperate 16 byte pixel splats.
       */
      u8x16_t pixel0_3 = __builtin_shufflevector(alive, kZero255,
        16, 0, 16, 17, 16, 1, 16, 17, 16, 2, 16, 17, 16, 3, 16, 17);
      u8x16_t pixel4_7 = __builtin_shufflevector(alive, kZero255,
        16, 4, 16, 17, 16, 5, 16, 17, 16, 6, 16, 17, 16, 7, 16, 17);
      u8x16_t pixel8_11 = __builtin_shufflevector(alive, kZero255,
        16, 8, 16, 17, 16, 9, 16, 17, 16, 10, 16, 17, 16, 11, 16, 17);
      u8x16_t pixel12_15 = __builtin_shufflevector(alive, kZero255,
        16, 12, 16, 17, 16, 13, 16, 17, 16, 14, 16, 17, 16, 15, 16, 17);

      /* Write 16 pixels to output pixel buffer. */
      *(u8x16_t*)(pixel_line + 0) = pixel0_3;
      *(u8x16_t*)(pixel_line + 4) = pixel4_7;
      *(u8x16_t*)(pixel_line + 8) = pixel8_11;
      *(u8x16_t*)(pixel_line + 12) = pixel12_15;

      /* Convert alive mask to 1 or 0 and store in destination cell array. */
      *(u8x16_t*)dst = alive & kOne;

      /* Increment pointers. */
      pixel_line += 16;
      dst += 16;
      src0 += 16;
      src1 += 16;
      src2 += 16;

      /* Shift source over by 16 cells and read the next 16 cells. */
      src00 = src01;
      src01 = *(u8x16_t*)&src0[16];
      src10 = src11;
      src11 = *(u8x16_t*)&src1[16];
      src20 = src21;
      src21 = *(u8x16_t*)&src2[16];
    }

    /*
     * The SIMD loop above does 16 cells at a time.  The loop below is the
     * regular version which processes one cell at a time.  It is used to
     * finish the remainder of the scanline not handled by the SIMD loop.
     */
    for (; x < (g_Context.size.width - 1); ++x) {
      /* Sum the jittered sources to construct neighbor count. */
      int count = src0[0] + src0[1] + src0[2] +
                  src1[0] +         + src1[2] +
                  src2[0] + src2[1] + src2[2];
      /* Add the center cell. */
      count = count + count + src1[1];
      /* Use table lookup indexed by count to determine pixel & alive state. */
      uint32_t color = kNeighborColors[count];
      *pixel_line++ = color;
      *dst++ = kIsAlive[count];
      ++src0;
      ++src1;
      ++src2;
    }
  }

  cell_temp = g_Context.cell_in;
  g_Context.cell_in = g_Context.cell_out;
  g_Context.cell_out = cell_temp;

  /* Unmap the range, we no longer need it. */
  g_pImageData->Unmap(image);

  /* Replace the contexts, and block until it's on the screen. */
  g_pGraphics2D->ReplaceContents(g_Context.ctx, image);
  g_pGraphics2D->Flush(g_Context.ctx, PP_BlockUntilComplete());

  /* Release the image data, we no longer need it. */
  g_pCore->ReleaseResource(image);
}

/*
 * Starting point for the module.  We do not use main since it would
 * collide with main in libppapi_cpp.
 */
int example_main(int argc, char *argv[]) {
  fprintf(stdout,"Started main.\n");
  g_pCore = (PPB_Core*)PSGetInterface(PPB_CORE_INTERFACE);
  g_pFullscreen = (PPB_Fullscreen*)PSGetInterface(PPB_FULLSCREEN_INTERFACE);
  g_pGraphics2D = (PPB_Graphics2D*)PSGetInterface(PPB_GRAPHICS_2D_INTERFACE);
  g_pInstance = (PPB_Instance*)PSGetInterface(PPB_INSTANCE_INTERFACE);
  g_pImageData = (PPB_ImageData*)PSGetInterface(PPB_IMAGEDATA_INTERFACE);
  g_pView = (PPB_View*)PSGetInterface(PPB_VIEW_INTERFACE);

  g_pInputEvent =
      (PPB_InputEvent*) PSGetInterface(PPB_INPUT_EVENT_INTERFACE);
  g_pKeyboardInput = (PPB_KeyboardInputEvent*)
      PSGetInterface(PPB_KEYBOARD_INPUT_EVENT_INTERFACE);
  g_pMouseInput =
      (PPB_MouseInputEvent*) PSGetInterface(PPB_MOUSE_INPUT_EVENT_INTERFACE);
  g_pTouchInput =
      (PPB_TouchInputEvent*) PSGetInterface(PPB_TOUCH_INPUT_EVENT_INTERFACE);

  PSEventSetFilter(PSE_ALL);
  while (1) {
    /* Process all waiting events without blocking */
    PSEvent* event;
    while ((event = PSEventTryAcquire()) != NULL) {
      ProcessEvent(event);
      PSEventRelease(event);
    }

    /* Render a frame, blocking until complete. */
    if (g_Context.bound) {
      Render();
    }
  }
  return 0;
}

/*
 * Register the function to call once the Instance Object is initialized.
 * see: pappi_simple/ps_main.h
 */
PPAPI_SIMPLE_REGISTER_MAIN(example_main);
