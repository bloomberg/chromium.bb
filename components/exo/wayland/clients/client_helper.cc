// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/client_helper.h"

#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"

#if defined(USE_GBM)
#include <gbm.h>
#endif

// Convenient macro that is used to define default deleters for object
// types allowing them to be used with std::unique_ptr.
#define DEFAULT_DELETER(TypeName, DeleteFunction)            \
  namespace std {                                            \
  void default_delete<TypeName>::operator()(TypeName* ptr) { \
    DeleteFunction(ptr);                                     \
  }                                                          \
  }

DEFAULT_DELETER(wl_buffer, wl_buffer_destroy)
DEFAULT_DELETER(wl_callback, wl_callback_destroy)
DEFAULT_DELETER(wl_compositor, wl_compositor_destroy)
DEFAULT_DELETER(wl_display, wl_display_disconnect)
DEFAULT_DELETER(wl_pointer, wl_pointer_destroy)
DEFAULT_DELETER(wl_region, wl_region_destroy)
DEFAULT_DELETER(wl_seat, wl_seat_destroy)
DEFAULT_DELETER(wl_shell, wl_shell_destroy)
DEFAULT_DELETER(wl_shell_surface, wl_shell_surface_destroy)
DEFAULT_DELETER(wl_shm, wl_shm_destroy)
DEFAULT_DELETER(wl_shm_pool, wl_shm_pool_destroy)
DEFAULT_DELETER(wl_subcompositor, wl_subcompositor_destroy)
DEFAULT_DELETER(wl_subsurface, wl_subsurface_destroy)
DEFAULT_DELETER(wl_surface, wl_surface_destroy)
DEFAULT_DELETER(wl_touch, wl_touch_destroy)
DEFAULT_DELETER(wp_presentation, wp_presentation_destroy)
DEFAULT_DELETER(struct wp_presentation_feedback,
                wp_presentation_feedback_destroy)
DEFAULT_DELETER(zaura_shell, zaura_shell_destroy)
DEFAULT_DELETER(zaura_surface, zaura_surface_destroy)
DEFAULT_DELETER(zwp_linux_buffer_params_v1, zwp_linux_buffer_params_v1_destroy)
DEFAULT_DELETER(zwp_linux_dmabuf_v1, zwp_linux_dmabuf_v1_destroy)

#if defined(USE_GBM)
DEFAULT_DELETER(gbm_bo, gbm_bo_destroy)
DEFAULT_DELETER(gbm_device, gbm_device_destroy)
#endif

namespace exo {
namespace wayland {
namespace clients {

#if defined(USE_GBM)
GLuint DeleteTextureTraits::InvalidValue() {
  return 0;
}
void DeleteTextureTraits::Free(GLuint texture) {
  glDeleteTextures(1, &texture);
}

EGLImageKHR DeleteEglImageTraits::InvalidValue() {
  return 0;
}
void DeleteEglImageTraits::Free(EGLImageKHR image) {
  eglDestroyImageKHR(eglGetCurrentDisplay(), image);
}

EGLSyncKHR DeleteEglSyncTraits::InvalidValue() {
  return 0;
}
void DeleteEglSyncTraits::Free(EGLSyncKHR sync) {
  eglDestroySyncKHR(eglGetCurrentDisplay(), sync);
}
#endif

}  // namespace clients
}  // namespace wayland
}  // namespace exo
