// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/avatar_label.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/taskbar_decorator.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view),
      avatar_button_(NULL),
      avatar_label_(NULL) {
}

BrowserNonClientFrameView::~BrowserNonClientFrameView() {
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

void BrowserNonClientFrameView::OnThemeChanged() {
  if (avatar_label_)
    avatar_label_->UpdateLabelStyle();
}

void BrowserNonClientFrameView::UpdateAvatarInfo() {
  if (browser_view_->ShouldShowAvatar()) {
    if (!avatar_button_) {
      Profile* profile = browser_view_->browser()->profile();
      if (profile->IsManaged() && !avatar_label_) {
        avatar_label_ = new AvatarLabel(browser_view_);
        avatar_label_->set_id(VIEW_ID_AVATAR_LABEL);
        AddChildView(avatar_label_);
      }
      avatar_button_ = new AvatarMenuButton(
          browser_view_->browser(),
          browser_view_->IsOffTheRecord() && !browser_view_->IsGuestSession());
      avatar_button_->set_id(VIEW_ID_AVATAR_BUTTON);
      AddChildView(avatar_button_);
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_) {
    // The avatar label can just be there if there is also an avatar button.
    if (avatar_label_) {
      RemoveChildView(avatar_label_);
      delete avatar_label_;
      avatar_label_ = NULL;
    }
    RemoveChildView(avatar_button_);
    delete avatar_button_;
    avatar_button_ = NULL;
    frame_->GetRootView()->Layout();
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Image avatar;
  string16 text;
  bool is_rectangle = false;
  if (browser_view_->IsGuestSession()) {
    avatar = rb.GetImageNamed(browser_view_->GetGuestIconResourceID());
  } else if (browser_view_->IsOffTheRecord()) {
    avatar = rb.GetImageNamed(browser_view_->GetOTRIconResourceID());
  } else if (AvatarMenu::ShouldShowAvatarMenu()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    Profile* profile = browser_view_->browser()->profile();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index == std::string::npos)
      return;
    text = cache.GetNameOfProfileAtIndex(index);

    AvatarMenu::GetImageForMenuButton(browser_view_->browser()->profile(),
                                      &avatar,
                                      &is_rectangle);
  }
  if (avatar_button_) {
    avatar_button_->SetAvatarIcon(avatar, is_rectangle);
    if (!text.empty())
      avatar_button_->SetText(text);
  }

  // For popups and panels which don't have the avatar button, we still
  // need to draw the taskbar decoration.
  chrome::DrawTaskbarDecoration(
      frame_->GetNativeWindow(),
      AvatarMenu::ShouldShowAvatarMenu() ? &avatar : NULL);
}
