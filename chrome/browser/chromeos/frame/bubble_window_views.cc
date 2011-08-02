// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_window_views.h"

#include <gtk/gtk.h>

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "views/widget/widget_delegate.h"
#include "views/window/non_client_view.h"


namespace chromeos {

BubbleWindowViews::BubbleWindowViews(BubbleWindowStyle style)
    : style_(style) {
}

views::NonClientFrameView* BubbleWindowViews::CreateNonClientFrameView() {
  return new BubbleFrameView(this, widget_delegate(), style_);
}

views::Widget* BubbleWindowViews::Create(
    gfx::NativeWindow parent,
    BubbleWindowStyle style,
    views::WidgetDelegate* widget_delegate) {
  BubbleWindowViews* window = new BubbleWindowViews(style);
  views::Widget::InitParams params;
  params.delegate = widget_delegate;
  params.parent = GTK_WIDGET(parent);
  params.bounds = gfx::Rect();
  params.transparent = true;
  window->Init(params);

  // TODO(saintlou): After discussions with mazda we concluded that we could
  // punt on an initial implementation of this style which is only used when
  // displaying the keyboard overlay. Once we have implemented gradient and
  // other missing features of Views we can address this.
  if (style == STYLE_XSHAPE)
    NOTIMPLEMENTED();

  return window;
}

}  // namespace chromeos
