// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_

#include <drm_fourcc.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_make_current.h"

namespace base {
class CommandLine;
}

namespace exo {
namespace wayland {
namespace clients {

class ClientBase {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();

    bool FromCommandLine(const base::CommandLine& command_line);

    std::string title = "Wayland Client";
    size_t num_buffers = 8;
    size_t width = 256;
    size_t height = 256;
    int scale = 1;
    bool fullscreen = false;
    bool transparent_background = false;
    bool use_drm = false;
    std::string use_drm_value;
    int32_t drm_format = DRM_FORMAT_ABGR8888;
  };

  struct Globals {
    Globals();
    ~Globals();

    std::unique_ptr<wl_compositor> compositor;
    std::unique_ptr<wl_shm> shm;
    std::unique_ptr<wp_presentation> presentation;
    std::unique_ptr<zwp_linux_dmabuf_v1> linux_dmabuf;
    std::unique_ptr<wl_shell> shell;
    std::unique_ptr<wl_seat> seat;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    std::unique_ptr<wl_buffer> buffer;
    bool busy = false;
#if defined(OZONE_PLATFORM_GBM)
    std::unique_ptr<gbm_bo> bo;
    std::unique_ptr<ScopedEglImage> egl_image;
    std::unique_ptr<ScopedEglSync> egl_sync;
    std::unique_ptr<ScopedTexture> texture;
#endif
    std::unique_ptr<zwp_linux_buffer_params_v1> params;
    std::unique_ptr<base::SharedMemory> shared_memory;
    std::unique_ptr<wl_shm_pool> shm_pool;
    sk_sp<SkSurface> sk_surface;
  };

  bool Init(const InitParams& params);

 protected:
  ClientBase();
  virtual ~ClientBase();

  size_t width_ = 256;
  size_t height_ = 256;
  int scale_ = 1;
  bool fullscreen_ = false;
  bool transparent_background_ = false;

  std::unique_ptr<wl_display> display_;
  std::unique_ptr<wl_surface> surface_;
  std::unique_ptr<wl_shell_surface> shell_surface_;
  Globals globals_;
#if defined(OZONE_PLATFORM_GBM)
  base::ScopedFD drm_fd_;
  std::unique_ptr<gbm_device> device_;
#endif
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<ui::ScopedMakeCurrent> make_current_;
  unsigned egl_sync_type_ = 0;
  std::vector<std::unique_ptr<Buffer>> buffers_;
  sk_sp<GrContext> gr_context_;

 private:
  std::unique_ptr<Buffer> CreateBuffer(int32_t drm_format);

  DISALLOW_COPY_AND_ASSIGN(ClientBase);
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
