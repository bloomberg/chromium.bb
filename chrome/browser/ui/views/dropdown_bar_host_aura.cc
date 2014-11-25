// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include "ui/aura/window.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
                                              bool no_redraw) {
  if (!host_->IsVisible())
    host_->GetNativeView()->Show();
  host_->GetNativeView()->SetBounds(new_pos);

  // The z-order of |host_| is controlled by the view specified via
  // views::kHostViewKey.
}

void DropdownBarHost::SetHostViewNative(views::View* host_view) {
  host_->GetNativeView()->SetProperty(views::kHostViewKey, host_view);
}
