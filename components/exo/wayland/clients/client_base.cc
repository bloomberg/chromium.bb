// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/client_base.h"

#include <aura-shell-client-protocol.h>
#include <fcntl.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_GBM)
#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drm.h>

#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#endif

namespace exo {
namespace wayland {
namespace clients {
namespace switches {

// Specifies the client buffer size.
const char kSize[] = "size";

// Specifies the client scale factor (ie. number of physical pixels per DIP).
const char kScale[] = "scale";

// Specifies the client transform (ie. rotation).
const char kTransform[] = "transform";

// Specifies if the background should be transparent.
const char kTransparentBackground[] = "transparent-background";

// Use drm buffer instead of shared memory.
const char kUseDrm[] = "use-drm";

// Specifies if client should be fullscreen.
const char kFullscreen[] = "fullscreen";

}  // namespace switches

namespace {

// Buffer format.
const int32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;
#if defined(USE_GBM)
const GrPixelConfig kGrPixelConfig = kBGRA_8888_GrPixelConfig;
#endif
const size_t kBytesPerPixel = 4;

#if defined(USE_GBM)
// DRI render node path template.
const char kDriRenderNodeTemplate[] = "/dev/dri/renderD%u";
#endif

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  ClientBase::Globals* globals = static_cast<ClientBase::Globals*>(data);

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
  } else if (strcmp(interface, "wp_presentation") == 0) {
    globals->presentation.reset(static_cast<wp_presentation*>(
        wl_registry_bind(registry, id, &wp_presentation_interface, 1)));
  } else if (strcmp(interface, "zaura_shell") == 0) {
    globals->aura_shell.reset(static_cast<zaura_shell*>(
        wl_registry_bind(registry, id, &zaura_shell_interface, 1)));
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
    globals->linux_dmabuf.reset(static_cast<zwp_linux_dmabuf_v1*>(
        wl_registry_bind(registry, id, &zwp_linux_dmabuf_v1_interface, 1)));
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    globals->subcompositor.reset(static_cast<wl_subcompositor*>(
        wl_registry_bind(registry, id, &wl_subcompositor_interface, 1)));
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

void BufferRelease(void* data, wl_buffer* /* buffer */) {
  ClientBase::Buffer* buffer = static_cast<ClientBase::Buffer*>(data);
  buffer->busy = false;
}

#if defined(USE_GBM)
const GrGLInterface* GrGLCreateNativeInterface() {
  return GrGLAssembleInterface(nullptr, [](void* ctx, const char name[]) {
    return eglGetProcAddress(name);
  });
}
#endif

wl_registry_listener g_registry_listener = {RegistryHandler, RegistryRemover};

wl_buffer_listener g_buffer_listener = {BufferRelease};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClientBase::InitParams, public:

ClientBase::InitParams::InitParams() {
#if defined(USE_GBM)
  drm_format = DRM_FORMAT_ABGR8888;
  bo_usage = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_TEXTURING;
#endif
}

ClientBase::InitParams::~InitParams() {}

bool ClientBase::InitParams::FromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSize)) {
    std::string size_str = command_line.GetSwitchValueASCII(switches::kSize);
    if (sscanf(size_str.c_str(), "%zdx%zd", &width, &height) != 2) {
      LOG(ERROR) << "Invalid value for " << switches::kSize;
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kScale) &&
      !base::StringToInt(command_line.GetSwitchValueASCII(switches::kScale),
                         &scale)) {
    LOG(ERROR) << "Invalid value for " << switches::kScale;
    return false;
  }

  if (command_line.HasSwitch(switches::kTransform)) {
    std::string transform_str =
        command_line.GetSwitchValueASCII(switches::kTransform);
    if (transform_str == "0") {
      transform = WL_OUTPUT_TRANSFORM_NORMAL;
    } else if (transform_str == "90") {
      transform = WL_OUTPUT_TRANSFORM_90;
    } else if (transform_str == "180") {
      transform = WL_OUTPUT_TRANSFORM_180;
    } else if (transform_str == "270") {
      transform = WL_OUTPUT_TRANSFORM_270;
    } else {
      LOG(ERROR) << "Invalid value for " << switches::kTransform;
      return false;
    }
  }

  use_drm = command_line.HasSwitch(switches::kUseDrm);
  if (use_drm)
    use_drm_value = command_line.GetSwitchValueASCII(switches::kUseDrm);

  fullscreen = command_line.HasSwitch(switches::kFullscreen);
  transparent_background =
      command_line.HasSwitch(switches::kTransparentBackground);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ClientBase::Globals, public:

ClientBase::Globals::Globals() {}

ClientBase::Globals::~Globals() {}

////////////////////////////////////////////////////////////////////////////////
// ClientBase::Buffer, public:

ClientBase::Buffer::Buffer() {}

ClientBase::Buffer::~Buffer() {}

////////////////////////////////////////////////////////////////////////////////
// ClientBase, public:

bool ClientBase::Init(const InitParams& params) {
  size_.SetSize(params.width, params.height);
  scale_ = params.scale;
  transform_ = params.transform;
  switch (params.transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
      surface_size_.SetSize(params.width, params.height);
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      surface_size_.SetSize(params.height, params.width);
      break;
    default:
      NOTREACHED();
      break;
  }
  surface_size_ = gfx::ToCeiledSize(
      gfx::ScaleSize(gfx::SizeF(surface_size_), 1.0f / params.scale));
  fullscreen_ = params.fullscreen;
  transparent_background_ = params.transparent_background;

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "wl_display_connect failed";
    return false;
  }
  wl_registry* registry = wl_display_get_registry(display_.get());
  wl_registry_add_listener(registry, &g_registry_listener, &globals_);

  wl_display_roundtrip(display_.get());

  if (!globals_.compositor) {
    LOG(ERROR) << "Can't find compositor interface";
    return false;
  }
  if (!globals_.shm) {
    LOG(ERROR) << "Can't find shm interface";
    return false;
  }
  if (!globals_.presentation) {
    LOG(ERROR) << "Can't find presentation interface";
    return false;
  }
  if (params.use_drm && !globals_.linux_dmabuf) {
    LOG(ERROR) << "Can't find linux_dmabuf interface";
    return false;
  }
  if (!globals_.shell) {
    LOG(ERROR) << "Can't find shell interface";
    return false;
  }
  if (!globals_.seat) {
    LOG(ERROR) << "Can't find seat interface";
    return false;
  }
  if (!globals_.aura_shell) {
    LOG(ERROR) << "Can't find aura shell interface";
    return false;
  }

#if defined(USE_GBM)
  sk_sp<const GrGLInterface> native_interface;
  if (params.use_drm) {
    // Number of files to look for when discovering DRM devices.
    const uint32_t kDrmMaxMinor = 15;
    const uint32_t kRenderNodeStart = 128;
    const uint32_t kRenderNodeEnd = kRenderNodeStart + kDrmMaxMinor + 1;

    for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; i++) {
      std::string dri_render_node(
          base::StringPrintf(kDriRenderNodeTemplate, i));
      base::ScopedFD drm_fd(open(dri_render_node.c_str(), O_RDWR));
      if (drm_fd.get() < 0)
        continue;
      drmVersionPtr drm_version = drmGetVersion(drm_fd.get());
      if (!drm_version) {
        LOG(ERROR) << "Can't get version for device: '" << dri_render_node
                   << "'";
        return false;
      }
      if (strstr(drm_version->name, params.use_drm_value.c_str())) {
        drm_fd_ = std::move(drm_fd);
        break;
      }
    }

    if (drm_fd_.get() < 0) {
      LOG_IF(ERROR, params.use_drm)
          << "Can't find drm device: '" << params.use_drm_value << "'";
      LOG_IF(ERROR, !params.use_drm) << "Can't find drm device";
      return false;
    }

    device_.reset(gbm_create_device(drm_fd_.get()));
    if (!device_) {
      LOG(ERROR) << "Can't create gbm device";
      return false;
    }
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForGPU(params);
    bool gl_initialized = gl::init::InitializeGLOneOff();
    DCHECK(gl_initialized);
    gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    gl_context_ =
        gl::init::CreateGLContext(nullptr,  // share_group
                                  gl_surface_.get(), gl::GLContextAttribs());

    make_current_.reset(
        new ui::ScopedMakeCurrent(gl_context_.get(), gl_surface_.get()));

    if (gl::GLSurfaceEGL::HasEGLExtension("EGL_EXT_image_flush_external") ||
        gl::GLSurfaceEGL::HasEGLExtension("EGL_ARM_implicit_external_sync")) {
      egl_sync_type_ = EGL_SYNC_FENCE_KHR;
    }
    if (gl::GLSurfaceEGL::HasEGLExtension("EGL_ANDROID_native_fence_sync")) {
      egl_sync_type_ = EGL_SYNC_NATIVE_FENCE_ANDROID;
    }

    native_interface = sk_sp<const GrGLInterface>(GrGLCreateNativeInterface());
    DCHECK(native_interface);
    gr_context_ = sk_sp<GrContext>(GrContext::Create(
        kOpenGL_GrBackend,
        reinterpret_cast<GrBackendContext>(native_interface.get())));
    DCHECK(gr_context_);
  }
#endif
  for (size_t i = 0; i < params.num_buffers; ++i) {
    auto buffer = CreateBuffer(size_, params.drm_format, params.bo_usage);
    if (!buffer) {
      LOG(ERROR) << "Failed to create buffer";
      return false;
    }
    buffers_.push_back(std::move(buffer));
  }

  for (size_t i = 0; i < buffers_.size(); ++i) {
    // If the buffer handle doesn't exist, we would either be killed by the
    // server or die here.
    if (!buffers_[i]->buffer) {
      LOG(ERROR) << "buffer handle uninitialized.";
      return false;
    }
  }

  surface_.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));
  if (!surface_) {
    LOG(ERROR) << "Can't create surface";
    return false;
  }

  if (!transparent_background_) {
    std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
        wl_compositor_create_region(globals_.compositor.get())));
    if (!opaque_region) {
      LOG(ERROR) << "Can't create region";
      return false;
    }

    wl_region_add(opaque_region.get(), 0, 0, size_.width(), size_.height());
    wl_surface_set_opaque_region(surface_.get(), opaque_region.get());
  }
  std::unique_ptr<wl_shell_surface> shell_surface(
      static_cast<wl_shell_surface*>(
          wl_shell_get_shell_surface(globals_.shell.get(), surface_.get())));
  if (!shell_surface) {
    LOG(ERROR) << "Can't get shell surface";
    return false;
  }

  wl_shell_surface_set_title(shell_surface.get(), params.title.c_str());

  std::unique_ptr<zaura_surface> aura_surface(
      static_cast<zaura_surface*>(
          zaura_shell_get_aura_surface(globals_.aura_shell.get(),
                                       surface_.get())));
  if (!aura_surface) {
    LOG(ERROR) << "Can't get aura surface";
    return false;
  }

  zaura_surface_set_frame(aura_surface.get(), ZAURA_SURFACE_FRAME_TYPE_NORMAL);

  if (fullscreen_) {
    wl_shell_surface_set_fullscreen(shell_surface.get(),
                                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                    0, nullptr);
  } else {
    wl_shell_surface_set_toplevel(shell_surface.get());
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ClientBase, protected:

ClientBase::ClientBase() {}

ClientBase::~ClientBase() {}

////////////////////////////////////////////////////////////////////////////////
// ClientBase, private:

std::unique_ptr<ClientBase::Buffer> ClientBase::CreateBuffer(
    const gfx::Size& size,
    int32_t drm_format,
    int32_t bo_usage) {
  std::unique_ptr<Buffer> buffer;
#if defined(USE_GBM)
  if (device_) {
    buffer = CreateDrmBuffer(size, drm_format, bo_usage);
    CHECK(buffer) << "Can't create drm buffer";
  }
#endif

  if (!buffer) {
    buffer = std::make_unique<Buffer>();

    size_t stride = size.width() * kBytesPerPixel;
    buffer->shared_memory.reset(new base::SharedMemory());
    buffer->shared_memory->CreateAndMapAnonymous(stride * size.height());
    buffer->shm_pool.reset(wl_shm_create_pool(
        globals_.shm.get(), buffer->shared_memory->handle().GetHandle(),
        buffer->shared_memory->requested_size()));

    buffer->buffer.reset(static_cast<wl_buffer*>(
        wl_shm_pool_create_buffer(buffer->shm_pool.get(), 0, size.width(),
                                  size.height(), stride, kShmFormat)));
    if (!buffer->buffer) {
      LOG(ERROR) << "Can't create buffer";
      return nullptr;
    }

    buffer->sk_surface = SkSurface::MakeRasterDirect(
        SkImageInfo::Make(size.width(), size.height(), kColorType,
                          kOpaque_SkAlphaType),
        static_cast<uint8_t*>(buffer->shared_memory->memory()), stride);
    DCHECK(buffer->sk_surface);
  }

  wl_buffer_add_listener(buffer->buffer.get(), &g_buffer_listener,
                         buffer.get());
  return buffer;
}

std::unique_ptr<ClientBase::Buffer> ClientBase::CreateDrmBuffer(
    const gfx::Size& size,
    int32_t drm_format,
    int32_t bo_usage) {
  std::unique_ptr<Buffer> buffer;
#if defined(USE_GBM)
  if (device_) {
    buffer = std::make_unique<Buffer>();
    buffer->bo.reset(gbm_bo_create(device_.get(), size.width(), size.height(),
                                   drm_format, bo_usage));
    if (!buffer->bo) {
      LOG(ERROR) << "Can't create gbm buffer";
      return nullptr;
    }
    base::ScopedFD fd(gbm_bo_get_plane_fd(buffer->bo.get(), 0));

    buffer->params.reset(
        zwp_linux_dmabuf_v1_create_params(globals_.linux_dmabuf.get()));
    for (size_t i = 0; i < gbm_bo_get_num_planes(buffer->bo.get()); ++i) {
      base::ScopedFD fd(gbm_bo_get_plane_fd(buffer->bo.get(), i));
      uint32_t stride = gbm_bo_get_plane_stride(buffer->bo.get(), i);
      uint32_t offset = gbm_bo_get_plane_offset(buffer->bo.get(), i);
      zwp_linux_buffer_params_v1_add(buffer->params.get(), fd.get(), i, offset,
                                     stride, 0, 0);
    }
    buffer->buffer.reset(zwp_linux_buffer_params_v1_create_immed(
        buffer->params.get(), size.width(), size.height(), drm_format, 0));

    if (gbm_bo_get_num_planes(buffer->bo.get()) != 1)
      return buffer;

    EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                fd.get(),
                                EGL_WIDTH,
                                size.width(),
                                EGL_HEIGHT,
                                size.height(),
                                EGL_LINUX_DRM_FOURCC_EXT,
                                drm_format,
                                EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                gbm_bo_get_plane_stride(buffer->bo.get(), 0),
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
    GrBackendTexture backend_texture(size.width(), size.height(),
                                     kGrPixelConfig, texture_info);
    buffer->sk_surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context_.get(), backend_texture, kTopLeft_GrSurfaceOrigin,
        /* sampleCnt */ 0, /* colorSpace */ nullptr, /* props */ nullptr);
    DCHECK(buffer->sk_surface);
  }
#endif
  return buffer;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
