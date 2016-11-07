// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a client that produces output in the form of RGBA
// buffers when receiving pointer/touch events. RGB contains the lower
// 24 bits of the event timestamp and A is 0xff.

#include <fcntl.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <iostream>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_make_current.h"

#if defined(OZONE_PLATFORM_GBM)
#include <drm_fourcc.h>
#include <gbm.h>
#endif

// Convenient macro that is used to define default deleters for object
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
DEFAULT_DELETER(zwp_linux_buffer_params_v1, zwp_linux_buffer_params_v1_destroy)
DEFAULT_DELETER(zwp_linux_dmabuf_v1, zwp_linux_dmabuf_v1_destroy)

#if defined(OZONE_PLATFORM_GBM)
DEFAULT_DELETER(gbm_device, gbm_device_destroy)
DEFAULT_DELETER(gbm_bo, gbm_bo_destroy)
#endif

namespace exo {
namespace wayland {
namespace clients {
namespace {

// Title for the wayland client.
const char kClientTitle[] = "Motion Event Client";

// Buffer format.
const int32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;
#if defined(OZONE_PLATFORM_GBM)
const GrPixelConfig kGrPixelConfig = kBGRA_8888_GrPixelConfig;
const int32_t kDrmFormat = DRM_FORMAT_ABGR8888;
#endif
const size_t kBytesPerPixel = 4;

#if defined(OZONE_PLATFORM_GBM)
// DRI render node path.
const char kDriRenderNode[] = "/dev/dri/renderD128";
#endif

// Number of buffers.
const size_t kBuffers = 3;

// Rotation speed (degrees/second).
const double kRotationSpeed = 360.0;

// Benchmark interval in seconds.
const int kBenchmarkInterval = 5;

struct Globals {
  std::unique_ptr<wl_compositor> compositor;
  std::unique_ptr<wl_shm> shm;
  std::unique_ptr<zwp_linux_dmabuf_v1> linux_dmabuf;
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
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
    globals->linux_dmabuf.reset(static_cast<zwp_linux_dmabuf_v1*>(
        wl_registry_bind(registry, id, &zwp_linux_dmabuf_v1_interface, 1)));
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

#if defined(OZONE_PLATFORM_GBM)
struct DeleteTextureTraits {
  static GLuint InvalidValue() { return 0; }
  static void Free(GLuint texture) { glDeleteTextures(1, &texture); }
};
using ScopedTexture = base::ScopedGeneric<GLuint, DeleteTextureTraits>;

struct DeleteEglImageTraits {
  static EGLImageKHR InvalidValue() { return 0; }
  static void Free(EGLImageKHR image) {
    eglDestroyImageKHR(eglGetCurrentDisplay(), image);
  }
};
using ScopedEglImage = base::ScopedGeneric<EGLImageKHR, DeleteEglImageTraits>;
#endif

struct Buffer {
  std::unique_ptr<wl_buffer> buffer;
  bool busy = false;
#if defined(OZONE_PLATFORM_GBM)
  std::unique_ptr<gbm_bo> bo;
  std::unique_ptr<ScopedEglImage> egl_image;
  std::unique_ptr<ScopedTexture> texture;
#endif
  std::unique_ptr<zwp_linux_buffer_params_v1> params;
  base::SharedMemory shared_memory;
  std::unique_ptr<wl_shm_pool> shm_pool;
  sk_sp<SkSurface> sk_surface;
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

#if defined(OZONE_PLATFORM_GBM)
void LinuxBufferParamsCreated(void* data,
                              zwp_linux_buffer_params_v1* params,
                              wl_buffer* new_buffer) {
  Buffer* buffer = static_cast<Buffer*>(data);
  buffer->buffer.reset(new_buffer);
}

void LinuxBufferParamsFailed(void* data, zwp_linux_buffer_params_v1* params) {
  LOG(ERROR) << "Linux buffer params failed";
}

const GrGLInterface* GrGLCreateNativeInterface() {
  return GrGLAssembleInterface(nullptr, [](void* ctx, const char name[]) {
    return eglGetProcAddress(name);
  });
}
#endif

}  // namespace

class MotionEvents {
 public:
  MotionEvents(size_t width,
               size_t height,
               int scale,
               size_t num_rects,
               bool use_drm,
               bool fullscreen)
      : width_(width),
        height_(height),
        scale_(scale),
        num_rects_(num_rects),
        use_drm_(use_drm),
        fullscreen_(fullscreen) {}

  // Initialize and run client main loop.
  int Run();

 private:
  bool CreateBuffer(Buffer* buffer);
  size_t stride() const { return width_ * kBytesPerPixel; }
  size_t buffer_size() const { return stride() * height_; }

  const size_t width_;
  const size_t height_;
  const int scale_;
  const size_t num_rects_;
  const bool use_drm_;
  const bool fullscreen_;

  Globals globals_;
  std::unique_ptr<wl_display> display_;

#if defined(OZONE_PLATFORM_GBM)
  base::ScopedFD drm_fd_;
  std::unique_ptr<gbm_device> device_;
#endif

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<ui::ScopedMakeCurrent> make_current_;

  Buffer buffers_[kBuffers];
  sk_sp<GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(MotionEvents);
};

int MotionEvents::Run() {
  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "wl_display_connect failed";
    return 1;
  }

  bool gl_initialized = gl::init::InitializeGLOneOff();
  DCHECK(gl_initialized);
  gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  gl_context_ =
      gl::init::CreateGLContext(nullptr,  // share_group
                                gl_surface_.get(), gl::PreferIntegratedGpu);

  make_current_.reset(
      new ui::ScopedMakeCurrent(gl_context_.get(), gl_surface_.get()));
  wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};

  wl_registry* registry = wl_display_get_registry(display_.get());
  wl_registry_add_listener(registry, &registry_listener, &globals_);

  wl_display_roundtrip(display_.get());

  if (!globals_.compositor) {
    LOG(ERROR) << "Can't find compositor interface";
    return 1;
  }
  if (!globals_.shm) {
    LOG(ERROR) << "Can't find shm interface";
    return 1;
  }
  if (use_drm_ && !globals_.linux_dmabuf) {
    LOG(ERROR) << "Can't find linux_dmabuf interface";
    return 1;
  }
  if (!globals_.shell) {
    LOG(ERROR) << "Can't find shell interface";
    return 1;
  }
  if (!globals_.seat) {
    LOG(ERROR) << "Can't find seat interface";
    return 1;
  }

#if defined(OZONE_PLATFORM_GBM)
  drm_fd_.reset(open(kDriRenderNode, O_RDWR));
  if (drm_fd_.get() < 0) {
    LOG(ERROR) << "Can't open drm device '" << kDriRenderNode << "'";
    return 1;
  }

  device_.reset(gbm_create_device(drm_fd_.get()));
  if (!device_) {
    LOG(ERROR) << "Can't create gbm device";
    return 1;
  }
  sk_sp<const GrGLInterface> native_interface(GrGLCreateNativeInterface());
  DCHECK(native_interface);
  gr_context_ = sk_sp<GrContext>(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(native_interface.get())));
  DCHECK(gr_context_);
#endif

  wl_buffer_listener buffer_listener = {BufferRelease};

  for (size_t i = 0; i < kBuffers; ++i) {
    if (!CreateBuffer(&buffers_[i]))
      return 1;
  }
  wl_display_roundtrip(display_.get());
  for (size_t i = 0; i < kBuffers; ++i) {
    if (!buffers_[i].buffer)
      return 1;

    wl_buffer_add_listener(buffers_[i].buffer.get(), &buffer_listener,
                           &buffers_[i]);
  }

  std::unique_ptr<wl_surface> surface(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));
  if (!surface) {
    LOG(ERROR) << "Can't create surface";
    return 1;
  }

  std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
      wl_compositor_create_region(globals_.compositor.get())));
  if (!opaque_region) {
    LOG(ERROR) << "Can't create region";
    return 1;
  }

  wl_region_add(opaque_region.get(), 0, 0, width_, height_);
  wl_surface_set_opaque_region(surface.get(), opaque_region.get());

  std::unique_ptr<wl_shell_surface> shell_surface(
      static_cast<wl_shell_surface*>(
          wl_shell_get_shell_surface(globals_.shell.get(), surface.get())));
  if (!shell_surface) {
    LOG(ERROR) << "Can't get shell surface";
    return 1;
  }

  wl_shell_surface_set_title(shell_surface.get(), kClientTitle);

  if (fullscreen_) {
    wl_shell_surface_set_fullscreen(shell_surface.get(),
                                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                    0, nullptr);
  } else {
    wl_shell_surface_set_toplevel(shell_surface.get());
  }

  EventTimeStack event_times;

  std::unique_ptr<wl_pointer> pointer(
      static_cast<wl_pointer*>(wl_seat_get_pointer(globals_.seat.get())));
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
      static_cast<wl_touch*>(wl_seat_get_touch(globals_.seat.get())));
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
        std::find_if(std::begin(buffers_), std::end(buffers_),
                     [](const Buffer& buffer) { return !buffer.busy; });
    if (buffer == std::end(buffers_))
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
    if (gr_context_) {
      gr_context_->flush();
      glFinish();
    }
    wl_surface_set_buffer_scale(surface.get(), scale_);
    wl_surface_attach(surface.get(), buffer->buffer.get(), 0, 0);
    buffer->busy = true;

    frame_callback.reset(wl_surface_frame(surface.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener, &frame);
    frame.callback_pending = true;

    ++frames;

    wl_surface_commit(surface.get());
  } while (wl_display_dispatch(display_.get()) != -1);

  return 0;
}

bool MotionEvents::CreateBuffer(Buffer* buffer) {
#if defined(OZONE_PLATFORM_GBM)
  if (use_drm_) {
    buffer->bo.reset(gbm_bo_create(device_.get(), width_, height_, kDrmFormat,
                                   GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING));
    if (!buffer->bo) {
      LOG(ERROR) << "Can't create gbm buffer";
      return false;
    }
    base::ScopedFD fd(gbm_bo_get_plane_fd(buffer->bo.get(), 0));
    static const zwp_linux_buffer_params_v1_listener params_listener = {
        LinuxBufferParamsCreated, LinuxBufferParamsFailed};

    buffer->params.reset(
        zwp_linux_dmabuf_v1_create_params(globals_.linux_dmabuf.get()));
    zwp_linux_buffer_params_v1_add_listener(buffer->params.get(),
                                            &params_listener, buffer);
    zwp_linux_buffer_params_v1_add(buffer->params.get(), fd.get(), 0, 0,
                                   stride(), 0, 0);
    zwp_linux_buffer_params_v1_create(buffer->params.get(), width_, height_,
                                      kDrmFormat, 0);

    EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                fd.get(),
                                EGL_WIDTH,
                                width_,
                                EGL_HEIGHT,
                                height_,
                                EGL_LINUX_DRM_FOURCC_EXT,
                                kDrmFormat,
                                EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                stride(),
                                EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                0,
                                EGL_NONE};
    EGLImageKHR image = eglCreateImageKHR(
        eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
        nullptr /* no client buffer */, khr_image_attrs);

    buffer->egl_image.reset(new ScopedEglImage(image));
    GLuint texture = 0;
    glGenTextures(1, &texture);
    buffer->texture.reset(new ScopedTexture(texture));
    glBindTexture(GL_TEXTURE_2D, buffer->texture->get());
    glEGLImageTargetTexture2DOES(
        GL_TEXTURE_2D, static_cast<GLeglImageOES>(buffer->egl_image->get()));
    glBindTexture(GL_TEXTURE_2D, 0);

    GrGLTextureInfo texture_info;
    texture_info.fID = buffer->texture->get();
    texture_info.fTarget = GL_TEXTURE_2D;
    GrBackendTextureDesc desc;
    desc.fFlags = kRenderTarget_GrBackendTextureFlag;
    desc.fWidth = width_;
    desc.fHeight = height_;
    desc.fConfig = kGrPixelConfig;
    desc.fOrigin = kTopLeft_GrSurfaceOrigin;
    desc.fTextureHandle = reinterpret_cast<GrBackendObject>(&texture_info);

    buffer->sk_surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context_.get(), desc, nullptr);
    DCHECK(buffer->sk_surface);
    return true;
  }
#endif
  buffer->shared_memory.CreateAndMapAnonymous(buffer_size());
  buffer->shm_pool.reset(
      wl_shm_create_pool(globals_.shm.get(), buffer->shared_memory.handle().fd,
                         buffer->shared_memory.requested_size()));

  buffer->buffer.reset(static_cast<wl_buffer*>(wl_shm_pool_create_buffer(
      buffer->shm_pool.get(), 0, width_, height_, stride(), kShmFormat)));
  if (!buffer->buffer) {
    LOG(ERROR) << "Can't create buffer";
    return false;
  }

  buffer->sk_surface = SkSurface::MakeRasterDirect(
      SkImageInfo::Make(width_, height_, kColorType, kOpaque_SkAlphaType),
      static_cast<uint8_t*>(buffer->shared_memory.memory()), stride());
  DCHECK(buffer->sk_surface);
  return true;
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

// Use drm buffer instead of shared memory.
const char kUseDrm[] = "use-drm";

// Specifies if client should be fullscreen.
const char kFullscreen[] = "fullscreen";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  int width = 256;
  int height = 256;
  if (command_line->HasSwitch(switches::kSize)) {
    std::string size_str = command_line->GetSwitchValueASCII(switches::kSize);
    if (sscanf(size_str.c_str(), "%dx%d", &width, &height) != 2) {
      LOG(ERROR) << "Invalid value for " << switches::kSize;
      return 1;
    }
  }

  int scale = 1;
  if (command_line->HasSwitch(switches::kScale) &&
      !base::StringToInt(command_line->GetSwitchValueASCII(switches::kScale),
                         &scale)) {
    LOG(ERROR) << "Invalid value for " << switches::kScale;
    return 1;
  }

  size_t num_rects = 1;
  if (command_line->HasSwitch(switches::kNumRects) &&
      !base::StringToSizeT(
          command_line->GetSwitchValueASCII(switches::kNumRects), &num_rects)) {
    LOG(ERROR) << "Invalid value for " << switches::kNumRects;
    return 1;
  }

  bool use_drm = command_line->HasSwitch(switches::kUseDrm);
  bool fullscreen = command_line->HasSwitch(switches::kFullscreen);

  exo::wayland::clients::MotionEvents client(width, height, scale, num_rects,
                                             use_drm, fullscreen);
  return client.Run();
}
