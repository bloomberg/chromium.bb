// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/handle_web_keyboard_event.h"

#include "views/widget/widget.h"

void HandleWebKeyboardEvent(views::Widget* widget,
                            const NativeWebKeyboardEvent& event) {
  // TODO(saintlou): Handle event forwarding in Aura: crbug.com/99861.
  NOTIMPLEMENTED();
}
