// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/ime_event_guard.h"

#include "content/renderer/render_widget.h"

namespace content {

ImeEventGuard::ImeEventGuard(RenderWidget* widget)
  : ImeEventGuard(widget, false, true) {
}

ImeEventGuard::ImeEventGuard(RenderWidget* widget, bool show_ime, bool from_ime)
  : widget_(widget),
    show_ime_(show_ime),
    from_ime_(from_ime) {
  widget_->OnImeEventGuardStart(this);
}

ImeEventGuard::~ImeEventGuard() {
  widget_->OnImeEventGuardFinish(this);
}

} //  namespace content
