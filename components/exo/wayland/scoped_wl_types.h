// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SCOPED_WL_TYPES_H_
#define COMPONENTS_EXO_WAYLAND_SCOPED_WL_TYPES_H_

#include "base/scoped_generic.h"

extern "C" {
struct wl_display;
}

namespace exo {
namespace wayland {
namespace internal {

struct ScopedWLDisplayTraits {
  static wl_display* InvalidValue() { return nullptr; }
  static void Free(wl_display* display);
};

}  // namespace internal

using ScopedWLDisplay =
    base::ScopedGeneric<wl_display*, internal::ScopedWLDisplayTraits>;

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SCOPED_WL_TYPES_H_
