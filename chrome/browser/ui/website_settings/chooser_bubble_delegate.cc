// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "content/public/browser/web_contents.h"

ChooserBubbleDelegate::ChooserBubbleDelegate(content::RenderFrameHost* owner)
    : browser_(chrome::FindBrowserWithWebContents(
          content::WebContents::FromRenderFrameHost(owner))),
      owning_frame_(owner) {}

ChooserBubbleDelegate::~ChooserBubbleDelegate() {}

std::string ChooserBubbleDelegate::GetName() const {
  return "ChooserBubble";
}

const content::RenderFrameHost* ChooserBubbleDelegate::OwningFrame() const {
  return owning_frame_;
}
