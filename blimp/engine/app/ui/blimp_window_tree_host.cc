// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_window_tree_host.h"

#include "ui/platform_window/stub/stub_window.h"

namespace blimp {
namespace engine {

BlimpWindowTreeHost::BlimpWindowTreeHost() : aura::WindowTreeHostPlatform() {
  SetPlatformWindow(make_scoped_ptr(new ui::StubWindow(this)));
}

BlimpWindowTreeHost::~BlimpWindowTreeHost() {}

void BlimpWindowTreeHost::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  // Do nothing to avoid creating an output surface and gpu process.
}

}  // namespace engine
}  // namespace blimp
