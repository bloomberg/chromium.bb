// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

void BrowserBubble::InitPopup() {
  gfx::NativeView native_view = frame_->GetNativeView();
  views::WidgetGtk* pop = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  pop->SetOpacity(0xFF);
  pop->Init(native_view, bounds_);
  pop->SetContentsView(view_);
  popup_ = pop;
  Reposition();
  AttachToBrowser();
}

void BrowserBubble::MovePopup(int x, int y, int w, int h) {
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->SetBounds(gfx::Rect(x, y, w, h));
}

void BrowserBubble::Show(bool activate) {
  if (visible_)
    return;
  // TODO(port) respect activate flag.
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->Show();
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetGtk* pop = static_cast<views::WidgetGtk*>(popup_);
  pop->Hide();
  visible_ = false;
}
