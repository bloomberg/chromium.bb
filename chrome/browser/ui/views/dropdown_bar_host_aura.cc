// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

NativeWebKeyboardEvent DropdownBarHost::GetKeyboardEvent(
     const WebContents* contents,
     const views::KeyEvent& key_event) {
  return NativeWebKeyboardEvent(key_event.native_event());
}

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
                                              bool no_redraw) {
  if (!host_->IsVisible())
    host_->GetNativeView()->Show();
  host_->GetNativeView()->SetBounds(new_pos);
  host_->StackAtTop();
}
