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

void BubbleWindowViews::SetBackgroundColor() {
#if !defined(USE_AURA)
   // TODO(saintlou): Once Views are truly pure the code below needs to be
  // removed and replaced by the corresponding Views code.
  GdkColor background_color =
      gfx::SkColorToGdkColor(kBubbleWindowBackgroundColor);
  gtk_widget_modify_bg(GetNativeView(), GTK_STATE_NORMAL, &background_color);
#endif
}

views::NonClientFrameView* BubbleWindowViews::CreateNonClientFrameView() {
  return new BubbleFrameView(this, widget_delegate(), style_);
}

}  // namespace chromeos
