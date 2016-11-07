// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a client that produces output in the form of RGBA
// buffers when receiving pointer/touch events. RGB contains the lower
// 24 bits of the event timestamp and A is 0xff.

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <iostream>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
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

// Buffer format.
const int32_t kFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;
const size_t kBytesPerPixel = 4;

// Number of buffers.
const size_t kBuffers = 2;

// Rotation speed (degrees/second).
const double kRotationSpeed = 360.0;

// Benchmark interval in seconds.
const int kBenchmarkInterval = 5;

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

struct Buffer {
  std::unique_ptr<wl_buffer> buffer;
  sk_sp<SkSurface> sk_surface;
  bool busy = false;
};

void BufferRelease(void* data, wl_buffer* /* buffer */) {
  Buffer* buffer = static_cast<Buffer*>(data);

  buffer->busy = false;
}

using EventTimeStack = std::vector<uint32_t>;

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
  EventTimeStack* stack = static_cast<EventTimeStack*>(data);

  stack->push_back(time);
}

void PointerButton(void* data,
                   wl_pointer* pointer,
                   uint32_t serial,
                   uint32_t time,
                   uint32_t button,
                   uint32_t state) {}

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
  EventTimeStack* stack = static_cast<EventTimeStack*>(data);

  stack->push_back(time);
}

void TouchFrame(void* data, wl_touch* touch) {}

void TouchCancel(void* data, wl_touch* touch) {}

struct Frame {
  uint32_t time = 0;
  bool callback_pending = false;
};

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  Frame* frame = static_cast<Frame*>(data);

  static uint32_t initial_time = time;
  frame->time = time - initial_time;
  frame->callback_pending = false;
}

}  // namespace

class MotionEvents {
 public:
  MotionEvents(size_t width,
               size_t height,
               int scale,
               size_t num_rects,
               bool fullscreen)
      : width_(width),
        height_(height),
        scale_(scale),
        num_rects_(num_rects),
        fullscreen_(fullscreen) {}

  // Initialize and run client main loop.
  int Run();

 private:
  size_t stride() const { return width_ * kBytesPerPixel; }
  size_t buffer_size() const { return stride() * height_; }

  const size_t width_;
  const size_t height_;
  const int scale_;
  const size_t num_rects_;
  const bool fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(MotionEvents);
};

int MotionEvents::Run() {
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  if (!display) {
    LOG(ERROR) << "wl_display_connect failed";
    return 1;
  }

  wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};

  Globals globals;
  wl_registry* registry = wl_display_get_registry(display.get());
  wl_registry_add_listener(registry, &registry_listener, &globals);

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

  Buffer buffers[kBuffers];
  base::SharedMemory shared_memory;
  shared_memory.CreateAndMapAnonymous(buffer_size() * kBuffers);
  std::unique_ptr<wl_shm_pool> shm_pool(
      wl_shm_create_pool(globals.shm.get(), shared_memory.handle().fd,
                         shared_memory.requested_size()));
  for (size_t i = 0; i < kBuffers; ++i) {
    buffers[i].buffer.reset(static_cast<wl_buffer*>(
        wl_shm_pool_create_buffer(shm_pool.get(), i * buffer_size(), width_,
                                  height_, stride(), kFormat)));
    if (!buffers[i].buffer) {
      LOG(ERROR) << "Can't create buffer";
      return 1;
    }
    buffers[i].sk_surface = SkSurface::MakeRasterDirect(
        SkImageInfo::Make(width_, height_, kColorType, kOpaque_SkAlphaType),
        static_cast<uint8_t*>(shared_memory.memory()) + buffer_size() * i,
        stride());
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

  wl_region_add(opaque_region.get(), 0, 0, width_, height_);
  wl_surface_set_opaque_region(surface.get(), opaque_region.get());

  std::unique_ptr<wl_shell_surface> shell_surface(
      static_cast<wl_shell_surface*>(
          wl_shell_get_shell_surface(globals.shell.get(), surface.get())));
  if (!shell_surface) {
    LOG(ERROR) << "Can't get shell surface";
    return 1;
  }

  wl_shell_surface_set_title(shell_surface.get(), "Test Client");
  if (fullscreen_) {
    wl_shell_surface_set_fullscreen(shell_surface.get(),
                                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                    0, nullptr);
  } else {
    wl_shell_surface_set_toplevel(shell_surface.get());
  }

  EventTimeStack event_times;

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
  wl_pointer_add_listener(pointer.get(), &pointer_listener, &event_times);

  std::unique_ptr<wl_touch> touch(
      static_cast<wl_touch*>(wl_seat_get_touch(globals.seat.get())));
  if (!touch) {
    LOG(ERROR) << "Can't get touch";
    return 1;
  }

  wl_touch_listener touch_listener = {TouchDown, TouchUp, TouchMotion,
                                      TouchFrame, TouchCancel};
  wl_touch_add_listener(touch.get(), &touch_listener, &event_times);

  Frame frame;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  uint32_t frames = 0;
  base::TimeTicks benchmark_time = base::TimeTicks::Now();
  base::TimeDelta benchmark_interval =
      base::TimeDelta::FromSeconds(kBenchmarkInterval);

  do {
    if (frame.callback_pending)
      continue;

    Buffer* buffer =
        std::find_if(std::begin(buffers), std::end(buffers),
                     [](const Buffer& buffer) { return !buffer.busy; });
    if (buffer == std::end(buffers))
      continue;

    base::TimeTicks current_time = base::TimeTicks::Now();
    if ((current_time - benchmark_time) > benchmark_interval) {
      std::cout << frames << " frames in " << benchmark_interval.InSeconds()
                << " seconds: "
                << static_cast<double>(frames) / benchmark_interval.InSeconds()
                << " fps" << std::endl;
      benchmark_time = current_time;
      frames = 0;
    }

    SkCanvas* canvas = buffer->sk_surface->getCanvas();
    canvas->save();

    if (event_times.empty()) {
      canvas->clear(SK_ColorBLACK);
    } else {
      // Split buffer into one horizontal rectangle for each event received
      // since last frame. Latest event at the top.
      int y = 0;
      // Note: Rounding up to ensure we cover the whole canvas.
      int h = (height_ + (event_times.size() / 2)) / event_times.size();
      while (!event_times.empty()) {
        SkIRect rect = SkIRect::MakeXYWH(0, y, width_, h);
        SkPaint paint;
        paint.setColor(SkColorSetRGB((event_times.back() & 0x0000ff) >> 0,
                                     (event_times.back() & 0x00ff00) >> 8,
                                     (event_times.back() & 0xff0000) >> 16));
        canvas->drawIRect(rect, paint);
        event_times.pop_back();
        y += h;
      }
    }

    // Draw rotating rects.
    SkScalar half_width = SkScalarHalf(width_);
    SkScalar half_height = SkScalarHalf(height_);
    SkIRect rect =
        SkIRect::MakeXYWH(-SkScalarHalf(half_width), -SkScalarHalf(half_height),
                          half_width, half_height);
    SkScalar rotation = SkScalarMulDiv(frame.time, kRotationSpeed, 1000);
    SkPaint paint;
    canvas->translate(half_width, half_height);
    for (size_t i = 0; i < num_rects_; ++i) {
      const SkColor kColors[] = {SK_ColorBLUE, SK_ColorGREEN,
                                 SK_ColorRED,  SK_ColorYELLOW,
                                 SK_ColorCYAN, SK_ColorMAGENTA};
      paint.setColor(SkColorSetA(kColors[i % arraysize(kColors)], 0xA0));
      canvas->rotate(rotation / num_rects_);
      canvas->drawIRect(rect, paint);
    }
    canvas->restore();

    wl_surface_set_buffer_scale(surface.get(), scale_);
    wl_surface_attach(surface.get(), buffer->buffer.get(), 0, 0);
    buffer->busy = true;

    frame_callback.reset(wl_surface_frame(surface.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener, &frame);
    frame.callback_pending = true;

    ++frames;

    wl_surface_commit(surface.get());
  } while (wl_display_dispatch(display.get()) != -1);

  return 0;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

namespace switches {

// Specifies the client buffer size.
const char kSize[] = "size";

// Specifies the client scale factor (ie. number of physical pixels per DIP).
const char kScale[] = "scale";

// Specifies the number of rotating rects to draw.
const char kNumRects[] = "num-rects";

// Specifies if client should be fullscreen.
const char kFullscreen[] = "fullscreen";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::CommandLine command_line(argc, argv);

  int width = 256;
  int height = 256;
  if (command_line.HasSwitch(switches::kSize)) {
    std::string size_str = command_line.GetSwitchValueASCII(switches::kSize);
    if (sscanf(size_str.c_str(), "%dx%d", &width, &height) != 2) {
      LOG(ERROR) << "Invalid value for " << switches::kSize;
      return 1;
    }
  }

  int scale = 1;
  if (command_line.HasSwitch(switches::kScale) &&
      !base::StringToInt(command_line.GetSwitchValueASCII(switches::kScale),
                         &scale)) {
    LOG(ERROR) << "Invalid value for " << switches::kScale;
    return 1;
  }

  size_t num_rects = 1;
  if (command_line.HasSwitch(switches::kNumRects) &&
      !base::StringToSizeT(
          command_line.GetSwitchValueASCII(switches::kNumRects), &num_rects)) {
    LOG(ERROR) << "Invalid value for " << switches::kNumRects;
    return 1;
  }

  bool fullscreen = command_line.HasSwitch(switches::kFullscreen);

  exo::wayland::clients::MotionEvents client(width, height, scale, num_rects,
                                             fullscreen);
  return client.Run();
}
