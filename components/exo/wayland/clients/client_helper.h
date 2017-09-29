// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_

#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <memory>

#include "base/scoped_generic.h"
#include "components/exo/wayland/aura-shell-client-protocol.h"

#if defined(USE_GBM)
#include <gbm.h>
#endif

// Default deleters template specialization forward decl.
#define DEFAULT_DELETER_FDECL(TypeName) \
  namespace std {                       \
  template <>                           \
  struct default_delete<TypeName> {     \
    void operator()(TypeName* ptr);     \
  };                                    \
  }

DEFAULT_DELETER_FDECL(wl_buffer)
DEFAULT_DELETER_FDECL(wl_callback)
DEFAULT_DELETER_FDECL(wl_compositor)
DEFAULT_DELETER_FDECL(wl_display)
DEFAULT_DELETER_FDECL(wl_pointer)
DEFAULT_DELETER_FDECL(wl_region)
DEFAULT_DELETER_FDECL(wl_seat)
DEFAULT_DELETER_FDECL(wl_shell)
DEFAULT_DELETER_FDECL(wl_shell_surface)
DEFAULT_DELETER_FDECL(wl_shm)
DEFAULT_DELETER_FDECL(wl_shm_pool)
DEFAULT_DELETER_FDECL(wl_subcompositor)
DEFAULT_DELETER_FDECL(wl_subsurface)
DEFAULT_DELETER_FDECL(wl_surface)
DEFAULT_DELETER_FDECL(wl_touch)
DEFAULT_DELETER_FDECL(wp_presentation)
DEFAULT_DELETER_FDECL(struct wp_presentation_feedback)
DEFAULT_DELETER_FDECL(zaura_shell)
DEFAULT_DELETER_FDECL(zaura_surface)
DEFAULT_DELETER_FDECL(zwp_linux_buffer_params_v1)
DEFAULT_DELETER_FDECL(zwp_linux_dmabuf_v1)

#if defined(USE_GBM)
DEFAULT_DELETER_FDECL(gbm_bo)
DEFAULT_DELETER_FDECL(gbm_device)
#endif

namespace exo {
namespace wayland {
namespace clients {

#if defined(USE_GBM)
struct DeleteTextureTraits {
  static unsigned InvalidValue();
  static void Free(unsigned texture);
};
using ScopedTexture = base::ScopedGeneric<unsigned, DeleteTextureTraits>;

struct DeleteEglImageTraits {
  static void* InvalidValue();
  static void Free(void* image);
};
using ScopedEglImage = base::ScopedGeneric<void*, DeleteEglImageTraits>;

struct DeleteEglSyncTraits {
  static void* InvalidValue();
  static void Free(void* sync);
};
using ScopedEglSync = base::ScopedGeneric<void*, DeleteEglSyncTraits>;
#endif

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_
