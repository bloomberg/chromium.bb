// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_window.h"

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"

namespace chromeos {

// static
const SkColor kBubbleWindowBackgroundColor = SK_ColorWHITE;

views::Widget* BubbleWindow::Create(
    gfx::NativeWindow parent,
    DialogStyle style,
    views::WidgetDelegate* widget_delegate) {
  BubbleWindow* window = new BubbleWindow(style);
  views::Widget::InitParams params;
  params.delegate = widget_delegate;
  params.parent = reinterpret_cast<gfx::NativeView>(parent);
  window->Init(params);
  return window;
}

BubbleWindow::~BubbleWindow() {
}

views::NonClientFrameView* BubbleWindow::CreateNonClientFrameView() {
  return new BubbleFrameView(widget_delegate(), style_);
}

BubbleWindow::BubbleWindow(DialogStyle style)
    : style_(style) {
}

}  // namespace chromeos
