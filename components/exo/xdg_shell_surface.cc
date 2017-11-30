// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/xdg_shell_surface.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// XdgShellSurface, public:

XdgShellSurface::XdgShellSurface(Surface* surface,
                                 BoundsMode bounds_mode,
                                 const gfx::Point& origin,
                                 bool activatable,
                                 bool can_minimize,
                                 int container)
    : ShellSurface(surface,
                   bounds_mode,
                   origin,
                   activatable,
                   can_minimize,
                   container) {}

XdgShellSurface::~XdgShellSurface() {}

void XdgShellSurface::SetBoundsMode(BoundsMode mode) {
  TRACE_EVENT1("exo", "XdgShellSurface::SetBoundsMode", "mode",
               static_cast<int>(mode));

  bounds_mode_ = mode;
}

}  // namespace exo
