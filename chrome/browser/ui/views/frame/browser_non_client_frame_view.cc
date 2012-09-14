// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
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
      avatar_button_.reset(
          new AvatarMenuButton(browser_view_->browser(),
                               browser_view_->IsOffTheRecord()));
      AddChildView(avatar_button_.get());
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_.get()) {
    RemoveChildView(avatar_button_.get());
    avatar_button_.reset();
    frame_->GetRootView()->Layout();
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Image avatar;
  string16 text;
  bool is_gaia_picture = false;
  if (browser_view_->IsOffTheRecord()) {
    avatar = rb.GetImageNamed(browser_view_->GetOTRIconResourceID());
  } else if (ManagedMode::IsInManagedMode()) {
    avatar = rb.GetImageNamed(IDR_MANAGED_MODE_AVATAR);
  } else if (AvatarMenuModel::ShouldShowAvatarMenu()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    Profile* profile = browser_view_->browser()->profile();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index == std::string::npos)
      return;
    is_gaia_picture =
        cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
        cache.GetGAIAPictureOfProfileAtIndex(index);
    avatar = cache.GetAvatarIconOfProfileAtIndex(index);
    text = cache.GetNameOfProfileAtIndex(index);
  }
  if (avatar_button_.get()) {
    avatar_button_->SetAvatarIcon(avatar, is_gaia_picture);
    if (!text.empty())
      avatar_button_->SetText(text);
  }

  // For popups and panels which don't have the avatar button, we still
  // need to draw the taskbar decoration.
  if (AvatarMenuModel::ShouldShowAvatarMenu() ||
      ManagedMode::IsInManagedMode()) {
    DrawTaskBarDecoration(frame_->GetNativeWindow(), &avatar);
  } else {
    DrawTaskBarDecoration(frame_->GetNativeWindow(), NULL);
  }
}

void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  if (!is_visible)
    return;
  // The first time UpdateAvatarInfo() is called the window is not visible so
  // DrawTaskBarDecoration() has no effect. Therefore we need to call it again
  // once the window is visible.
  UpdateAvatarInfo();
}
