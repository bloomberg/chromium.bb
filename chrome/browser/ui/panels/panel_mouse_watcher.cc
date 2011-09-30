// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_mouse_watcher.h"

#include "ui/gfx/point.h"

PanelMouseWatcher::PanelMouseWatcher(Observer* observer)
    : observer_(observer) {
}

PanelMouseWatcher::~PanelMouseWatcher() {
}

void PanelMouseWatcher::NotifyMouseMovement(const gfx::Point& mouse_position) {
  observer_->OnMouseMove(mouse_position);
}
