// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a client that produces output in the form of RGBA
// buffers when receiving pointer/touch events. RGB contains the lower
// 24 bits of the event timestamp and A is 0xff.

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"

// Convenient macro that is used to define default deleters for wayland object
// types allowing them to be used with std::unique_ptr.
#define DEFAULT_DELETER(TypeName, DeleteFunction)           \
  namespace std {                                           \
  template <>                                               \
  struct default_delete<TypeName> {                         \
    void operator()(TypeName* ptr) { DeleteFunction(ptr); } \
  };                                                        \
  }

DEFAULT_DELETER(wl_display, wl_display_disconnect)
DEFAULT_DELETER(wl_compositor, wl_compositor_destroy)
DEFAULT_DELETER(wl_shm, wl_shm_destroy)
DEFAULT_DELETER(wl_shm_pool, wl_shm_pool_destroy)
DEFAULT_DELETER(wl_buffer, wl_buffer_destroy)
DEFAULT_DELETER(wl_surface, wl_surface_destroy)
DEFAULT_DELETER(wl_region, wl_region_destroy)
DEFAULT_DELETER(wl_shell, wl_shell_destroy)
DEFAULT_DELETER(wl_shell_surface, wl_shell_surface_destroy)
DEFAULT_DELETER(wl_seat, wl_seat_destroy)
DEFAULT_DELETER(wl_pointer, wl_pointer_destroy)
DEFAULT_DELETER(wl_touch, wl_touch_destroy)
DEFAULT_DELETER(wl_callback, wl_callback_destroy)

namespace exo {
namespace wayland {
namespace clients {
namespace {

// Window size.
const size_t kWidth = 256;
const size_t kHeight = 256;

// Buffer format.
const int32_t kFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;
const size_t kBytesPerPixel = 4;

// Number of buffers.
const size_t kBuffers = 2;

// Rotation speed (degrees/second).
const double kRotationSpeed = 360.0;

// Helper constants.
const size_t kStride = kWidth * kBytesPerPixel;
const size_t kBufferSize = kHeight * kStride;
const size_t kMemorySize = kBufferSize * kBuffers;

struct Globals {
  std::unique_ptr<wl_compositor> compositor;
  std::unique_ptr<wl_shm> shm;
  std::unique_ptr<wl_shell> shell;
  std::unique_ptr<wl_seat> seat;
};

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  Globals* globals = static_cast<Globals*>(data);

  if (strcmp(interface, "wl_compositor") == 0) {
    globals->compositor.reset(static_cast<wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, 3)));
  } else if (strcmp(interface, "wl_shm") == 0) {
    globals->shm.reset(static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1)));
  } else if (strcmp(interface, "wl_shell") == 0) {
    globals->shell.reset(static_cast<wl_shell*>(
        wl_registry_bind(registry, id, &wl_shell_interface, 1)));
  } else if (strcmp(interface, "wl_seat") == 0) {
    globals->seat.reset(static_cast<wl_seat*>(
        wl_registry_bind(registry, id, &wl_seat_interface, 5)));
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

struct BufferState {
  std::unique_ptr<wl_buffer> buffer;
  sk_sp<SkSurface> sk_surface;
  bool busy = false;
};

void BufferRelease(void* data, wl_buffer* buffer) {
  BufferState* state = static_cast<BufferState*>(data);

  state->busy = false;
}

struct MainLoopContext {
  bool shutdown = false;
  uint32_t last_event_time = 0;
  bool frame_callback_pending = false;
  double rotation = 0.0;
};

void PointerEnter(void* data,
                  wl_pointer* pointer,
                  uint32_t serial,
                  wl_surface* surface,
                  wl_fixed_t x,
                  wl_fixed_t y) {}

void PointerLeave(void* data,
                  wl_pointer* pointer,
                  uint32_t serial,
                  wl_surface* surface) {}

void PointerMotion(void* data,
                   wl_pointer* pointer,
                   uint32_t time,
                   wl_fixed_t x,
                   wl_fixed_t y) {
  MainLoopContext* context = static_cast<MainLoopContext*>(data);

  context->last_event_time = time;
}

void PointerButton(void* data,
                   wl_pointer* pointer,
                   uint32_t serial,
                   uint32_t time,
                   uint32_t button,
                   uint32_t state) {
  MainLoopContext* context = static_cast<MainLoopContext*>(data);

  context->shutdown = true;
}

void PointerAxis(void* data,
                 wl_pointer* pointer,
                 uint32_t time,
                 uint32_t axis,
                 wl_fixed_t value) {}

void PointerAxisSource(void* data, wl_pointer* pointer, uint32_t axis_source) {}

void PointerAxisStop(void* data,
                     wl_pointer* pointer,
                     uint32_t time,
                     uint32_t axis) {}

void PointerDiscrete(void* data,
                     wl_pointer* pointer,
                     uint32_t axis,
                     int32_t discrete) {}

void PointerFrame(void* data, wl_pointer* pointer) {}

void TouchDown(void* data,
               wl_touch* touch,
               uint32_t serial,
               uint32_t time,
               wl_surface* surface,
               int32_t id,
               wl_fixed_t x,
               wl_fixed_t y) {}

void TouchUp(void* data,
             wl_touch* touch,
             uint32_t serial,
             uint32_t time,
             int32_t id) {}

void TouchMotion(void* data,
                 wl_touch* touch,
                 uint32_t time,
                 int32_t id,
                 wl_fixed_t x,
                 wl_fixed_t y) {
  MainLoopContext* context = static_cast<MainLoopContext*>(data);

  context->last_event_time = time;
}

void TouchFrame(void* data, wl_touch* touch) {}

void TouchCancel(void* data, wl_touch* touch) {}

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  MainLoopContext* context = static_cast<MainLoopContext*>(data);

  static uint32_t initial_time = time;
  context->rotation = ((time - initial_time) / 1000.0) * kRotationSpeed;
  context->frame_callback_pending = false;
}

}  // namespace

int MotionEventsMain() {
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  if (!display) {
    LOG(ERROR) << "wl_display_connect failed";
    return 1;
  }

  wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};

  Globals globals;
  wl_registry* registry = wl_display_get_registry(display.get());
  wl_registry_add_listener(registry, &registry_listener, &globals);

  wl_display_dispatch(display.get());
  wl_display_roundtrip(display.get());

  if (!globals.compositor) {
    LOG(ERROR) << "Can't find compositor interface";
    return 1;
  }
  if (!globals.shm) {
    LOG(ERROR) << "Can't find shm interface";
    return 1;
  }
  if (!globals.shell) {
    LOG(ERROR) << "Can't find shell interface";
    return 1;
  }
  if (!globals.seat) {
    LOG(ERROR) << "Can't find seat interface";
    return 1;
  }

  wl_buffer_listener buffer_listener = {BufferRelease};

  BufferState buffers[kBuffers];
  base::SharedMemory shared_memory;
  shared_memory.CreateAndMapAnonymous(kMemorySize);
  std::unique_ptr<wl_shm_pool> shm_pool(
      wl_shm_create_pool(globals.shm.get(), shared_memory.handle().fd,
                         shared_memory.requested_size()));
  for (size_t i = 0; i < kBuffers; ++i) {
    buffers[i].buffer.reset(static_cast<wl_buffer*>(wl_shm_pool_create_buffer(
        shm_pool.get(), i * kBufferSize, kWidth, kHeight, kStride, kFormat)));
    if (!buffers[i].buffer) {
      LOG(ERROR) << "Can't create buffer";
      return 1;
    }
    buffers[i].sk_surface = SkSurface::MakeRasterDirect(
        SkImageInfo::Make(kWidth, kHeight, kColorType, kUnpremul_SkAlphaType),
        static_cast<uint8_t*>(shared_memory.memory()) + kBufferSize * i,
        kStride);
    if (!buffers[i].sk_surface) {
      LOG(ERROR) << "Can't create SkSurface";
      return 1;
    }
    wl_buffer_add_listener(buffers[i].buffer.get(), &buffer_listener,
                           &buffers[i]);
  }

  std::unique_ptr<wl_surface> surface(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals.compositor.get())));
  if (!surface) {
    LOG(ERROR) << "Can't create surface";
    return 1;
  }

  std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
      wl_compositor_create_region(globals.compositor.get())));
  if (!opaque_region) {
    LOG(ERROR) << "Can't create region";
    return 1;
  }

  wl_region_add(opaque_region.get(), 0, 0, kWidth, kHeight);
  wl_surface_set_opaque_region(surface.get(), opaque_region.get());

  std::unique_ptr<wl_shell_surface> shell_surface(
      static_cast<wl_shell_surface*>(
          wl_shell_get_shell_surface(globals.shell.get(), surface.get())));
  if (!shell_surface) {
    LOG(ERROR) << "Can't get shell surface";
    return 1;
  }

  wl_shell_surface_set_title(shell_surface.get(), "Test Client");
  wl_shell_surface_set_toplevel(shell_surface.get());

  MainLoopContext context;

  std::unique_ptr<wl_pointer> pointer(
      static_cast<wl_pointer*>(wl_seat_get_pointer(globals.seat.get())));
  if (!pointer) {
    LOG(ERROR) << "Can't get pointer";
    return 1;
  }

  wl_pointer_listener pointer_listener = {
      PointerEnter,      PointerLeave,    PointerMotion,
      PointerButton,     PointerAxis,     PointerFrame,
      PointerAxisSource, PointerAxisStop, PointerDiscrete};
  wl_pointer_add_listener(pointer.get(), &pointer_listener, &context);

  std::unique_ptr<wl_touch> touch(
      static_cast<wl_touch*>(wl_seat_get_touch(globals.seat.get())));
  if (!touch) {
    LOG(ERROR) << "Can't get touch";
    return 1;
  }

  wl_touch_listener touch_listener = {TouchDown, TouchUp, TouchMotion,
                                      TouchFrame, TouchCancel};
  wl_touch_add_listener(touch.get(), &touch_listener, &context);

  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  do {
    if (context.shutdown)
      break;

    if (context.frame_callback_pending)
      continue;

    BufferState* buffer =
        std::find_if(std::begin(buffers), std::end(buffers),
                     [](const BufferState& buffer) { return !buffer.busy; });
    if (buffer == std::end(buffers))
      continue;

    SkCanvas* canvas = buffer->sk_surface->getCanvas();
    canvas->save();
    canvas->clear(SkColorSetRGB((context.last_event_time & 0x0000ff) >> 0,
                                (context.last_event_time & 0x00ff00) >> 8,
                                (context.last_event_time & 0xff0000) >> 16));
    canvas->translate(SkIntToScalar(kWidth / 2), SkIntToScalar(kHeight / 2));
    canvas->rotate(SkDoubleToScalar(context.rotation));
    SkRect rect = SkRect::MakeXYWH(-(kWidth / 4.0f), -(kHeight / 4.0f),
                                   kWidth / 2.0f, kHeight / 2.0f);
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    canvas->drawRect(rect, paint);
    canvas->restore();

    wl_surface_attach(surface.get(), buffer->buffer.get(), 0, 0);
    buffer->busy = true;

    frame_callback.reset(wl_surface_frame(surface.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener, &context);
    context.frame_callback_pending = true;

    wl_surface_commit(surface.get());
    wl_display_flush(display.get());
  } while (wl_display_dispatch(display.get()) != -1);

  return 0;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

int main() {
  return exo::wayland::clients::MotionEventsMain();
}
