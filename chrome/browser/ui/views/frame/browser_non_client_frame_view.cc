// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/gfx/image/image.h"

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view) {
}

BrowserNonClientFrameView::~BrowserNonClientFrameView() {
}

void BrowserNonClientFrameView::UpdateAvatarInfo() {
  if (browser_view_->ShouldShowAvatar()) {
    if (!avatar_button_.get()) {
      avatar_button_.reset(new AvatarMenuButton(
          browser_view_->browser(), !browser_view_->IsOffTheRecord()));
      AddChildView(avatar_button_.get());
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_.get()) {
    RemoveChildView(avatar_button_.get());
    avatar_button_.reset();
    frame_->GetRootView()->Layout();
  }

  if (!avatar_button_.get())
    return;

  if (browser_view_->IsOffTheRecord()) {
    avatar_button_->SetIcon(
        gfx::Image(new SkBitmap(browser_view_->GetOTRAvatarIcon())), false);
  } else {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    Profile* profile = browser_view_->browser()->profile();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index != std::string::npos) {
      bool is_gaia_picture =
          cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
          cache.GetGAIAPictureOfProfileAtIndex(index);
      avatar_button_->SetIcon(
          cache.GetAvatarIconOfProfileAtIndex(index), is_gaia_picture);
      avatar_button_->SetText(cache.GetNameOfProfileAtIndex(index));
    }
  }
}
