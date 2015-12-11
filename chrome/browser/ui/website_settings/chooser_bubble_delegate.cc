// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"

#if defined(OS_MACOSX)
#include "components/bubble/bubble_ui.h"
#endif

ChooserBubbleDelegate::ChooserBubbleDelegate(Browser* browser)
    : browser_(browser) {}

ChooserBubbleDelegate::~ChooserBubbleDelegate() {}

std::string ChooserBubbleDelegate::GetName() const {
  return "ChooserBubble";
}

// TODO(juncai): Add chooser bubble ui cocoa code for Mac.
// Please refer to http://crbug.com/492204 for more information.
#if defined(OS_MACOSX)
scoped_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return scoped_ptr<BubbleUi>();
}
#endif
