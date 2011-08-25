// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"

#include "aura/window.h"
#include "base/logging.h"
#include "ui/gfx/compositor/compositor.h"

namespace aura {

Desktop::Desktop(gfx::AcceleratedWidget widget, const gfx::Size& size)
    : compositor_(ui::Compositor::Create(widget, size)) {
  DCHECK(compositor_.get());
  window_.reset(new Window(this));
}

Desktop::~Desktop() {
}

void Desktop::Draw() {
  // Second pass renders the layers.
  compositor_->NotifyStart();
  window_->DrawTree();
  compositor_->NotifyEnd();
}

bool Desktop::OnMouseEvent(const MouseEvent& event) {
  return window_->OnMouseEvent(event);
}

}  // namespace aura
