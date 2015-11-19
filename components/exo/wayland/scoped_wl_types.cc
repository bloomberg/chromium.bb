// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/scoped_wl_types.h"

#include <wayland-server-core.h>

namespace exo {
namespace wayland {
namespace internal {

// static
void ScopedWLDisplayTraits::Free(wl_display* display) {
  wl_display_destroy(display);
}

}  // namespace internal
}  // namespace wayland
}  // namespace exo
