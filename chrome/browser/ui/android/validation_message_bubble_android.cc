// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/validation_message_bubble.h"

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text) {
  // TODO(tkent): Implement this and enable it. crbug.com/235721.
  return scoped_ptr<ValidationMessageBubble>().Pass();
}

}
