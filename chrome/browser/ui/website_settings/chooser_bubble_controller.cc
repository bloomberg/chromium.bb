// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/chooser_bubble_controller.h"

#include "chrome/browser/net/referrer.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

ChooserBubbleController::ChooserBubbleController(
    content::RenderFrameHost* owner)
    : browser_(chrome::FindBrowserWithWebContents(
          content::WebContents::FromRenderFrameHost(owner))),
      owning_frame_(owner) {}

ChooserBubbleController::~ChooserBubbleController() {}

url::Origin ChooserBubbleController::GetOrigin() const {
  return const_cast<content::RenderFrameHost*>(owning_frame_)
      ->GetLastCommittedOrigin();
}

void ChooserBubbleController::OpenHelpCenterUrl() const {
  browser_->OpenURL(content::OpenURLParams(
      GetHelpCenterUrl(), content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */));
}

std::string ChooserBubbleController::GetName() const {
  return "ChooserBubble";
}

const content::RenderFrameHost* ChooserBubbleController::OwningFrame() const {
  return owning_frame_;
}
