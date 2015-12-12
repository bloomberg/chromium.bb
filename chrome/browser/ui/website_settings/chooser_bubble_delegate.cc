// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"

ChooserBubbleDelegate::ChooserBubbleDelegate(Browser* browser)
    : browser_(browser) {}

ChooserBubbleDelegate::~ChooserBubbleDelegate() {}

std::string ChooserBubbleDelegate::GetName() const {
  return "ChooserBubble";
}
