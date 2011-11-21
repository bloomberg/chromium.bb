// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/handle_web_keyboard_event.h"

#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/views/widget/native_widget_gtk.h"

void HandleWebKeyboardEvent(views::Widget* widget,
                            const NativeWebKeyboardEvent& event) {
  if (widget && event.os_event && !event.skip_in_browser) {
    views::KeyEvent views_event(reinterpret_cast<GdkEvent*>(event.os_event));
    static_cast<views::NativeWidgetGtk*>(widget->native_widget())->
        HandleKeyboardEvent(views_event);
  }
}
