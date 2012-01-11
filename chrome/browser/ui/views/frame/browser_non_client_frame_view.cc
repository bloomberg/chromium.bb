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

  // For popups and panels which don't have the avatar button, we still
  // need to draw the taskbar decoration.
  if (browser_view_->IsBrowserTypeNormal()) {
    if (!avatar_button_.get())
      return;
  }

  if (browser_view_->IsGuestSession()) {
    const gfx::Image avatar(new SkBitmap(browser_view_->GetGuestAvatarIcon()));
    if (avatar_button_.get())
      avatar_button_->SetAvatarIcon(avatar, false);
    DrawTaskBarDecoration(frame_->GetNativeWindow(), &avatar);
  } else if (browser_view_->IsOffTheRecord()) {
    const gfx::Image avatar(new SkBitmap(browser_view_->GetOTRAvatarIcon()));
    if (avatar_button_.get())
      avatar_button_->SetAvatarIcon(avatar, false);
    DrawTaskBarDecoration(frame_->GetNativeWindow(), &avatar);
  } else {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    Profile* profile = browser_view_->browser()->profile();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index != std::string::npos) {
      bool is_gaia_picture =
          cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
          cache.GetGAIAPictureOfProfileAtIndex(index);
      const gfx::Image& avatar = cache.GetAvatarIconOfProfileAtIndex(index);
      if (avatar_button_.get()) {
        avatar_button_->SetAvatarIcon(avatar, is_gaia_picture);
        avatar_button_->SetText(cache.GetNameOfProfileAtIndex(index));
      }
      DrawTaskBarDecoration(frame_->GetNativeWindow(), &avatar);
    }
  }
}

#if defined(OS_WIN)
// See the comments in the .h file as for why this code is windows-only.
void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  if (!is_visible)
    return;
  // The first time UpdateAvatarInfo() is called the window is not visible so
  // DrawTaskBarDecoration() has no effect. Therefore we need to call it again
  // once the window is visible.
  UpdateAvatarInfo();
}
#endif
