// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"

#include <algorithm>

#include "aura/window.h"
#include "base/logging.h"
#include "ui/gfx/compositor/compositor.h"

namespace aura {

Desktop::Desktop(gfx::AcceleratedWidget widget, const gfx::Size& size)
    : compositor_(ui::Compositor::Create(widget, size)) {
  DCHECK(compositor_.get());
}

Desktop::~Desktop() {
}

void Desktop::Draw() {
  // First pass updates the layer bitmaps.
  for (Windows::iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->UpdateLayerCanvas();

  // Second pass renders the layers.
  compositor_->NotifyStart();
  for (Windows::iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->Draw();
  compositor_->NotifyEnd();
}

void Desktop::AddWindow(Window* window) {
  DCHECK(std::find(windows_.begin(), windows_.end(), window) == windows_.end());
  windows_.push_back(window);
}

void Desktop::RemoveWindow(Window* window) {
  Windows::iterator i = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(i != windows_.end());
  windows_.erase(i);
}

}  // namespace aura
