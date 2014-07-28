// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/taskbar_decorator.h"
#include "chrome/browser/ui/views/profiles/avatar_label.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "components/signin/core/common/profile_management_switches.h"
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
      avatar_label_(NULL),
      new_avatar_button_(NULL) {
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
  if (!browser_view_->IsRegularOrGuestSession() ||
      !switches::IsNewAvatarMenu())
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
      if (profile->IsSupervised() && !avatar_label_) {
        avatar_label_ = new AvatarLabel(browser_view_);
        avatar_label_->set_id(VIEW_ID_AVATAR_LABEL);
        AddChildView(avatar_label_);
      }
      avatar_button_ = new AvatarMenuButton(
          browser_view_->browser(), !browser_view_->IsRegularOrGuestSession());
      avatar_button_->set_id(VIEW_ID_AVATAR_BUTTON);
      AddChildView(avatar_button_);
      // Invalidate here because adding a child does not invalidate the layout.
      InvalidateLayout();
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
  gfx::Image taskbar_badge_avatar;
  base::string16 text;
  bool is_rectangle = false;
  if (browser_view_->IsGuestSession()) {
    avatar = rb.
        GetImageNamed(profiles::GetPlaceholderAvatarIconResourceID());
  } else if (browser_view_->IsOffTheRecord()) {
    avatar = rb.GetImageNamed(IDR_OTR_ICON);
    // TODO(nkostylev): Allow this on ChromeOS once the ChromeOS test
    // environment handles profile directories correctly.
#if !defined(OS_CHROMEOS)
    bool is_badge_rectangle = false;
    // The taskbar badge should be the profile avatar, not the OTR avatar.
    AvatarMenu::GetImageForMenuButton(browser_view_->browser()->profile(),
                                      &taskbar_badge_avatar,
                                      &is_badge_rectangle);
#endif
  } else if (avatar_button_ || AvatarMenu::ShouldShowAvatarMenu()) {
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
    // Disable the menu when we should not show the menu.
    if (avatar_button_ && !AvatarMenu::ShouldShowAvatarMenu())
      avatar_button_->SetEnabled(false);
  }
  if (avatar_button_)
    avatar_button_->SetAvatarIcon(avatar, is_rectangle);

  // For popups and panels which don't have the avatar button, we still
  // need to draw the taskbar decoration. Even though we have an icon on the
  // window's relaunch details, we draw over it because the user may have pinned
  // the badge-less Chrome shortcut which will cause windows to ignore the
  // relaunch details.
  // TODO(calamity): ideally this should not be necessary but due to issues with
  // the default shortcut being pinned, we add the runtime badge for safety.
  // See crbug.com/313800.
  chrome::DrawTaskbarDecoration(
      frame_->GetNativeWindow(),
      AvatarMenu::ShouldShowAvatarMenu()
          ? (taskbar_badge_avatar.IsEmpty() ? &avatar : &taskbar_badge_avatar)
          : NULL);
}

void BrowserNonClientFrameView::UpdateNewStyleAvatarInfo(
    views::ButtonListener* listener,
    const NewAvatarButton::AvatarButtonStyle style) {
  DCHECK(switches::IsNewAvatarMenu());
  // This should never be called in incognito mode.
  DCHECK(browser_view_->IsRegularOrGuestSession());

  if (browser_view_->ShouldShowAvatar()) {
    if (!new_avatar_button_) {
      base::string16 profile_name = profiles::GetAvatarNameForProfile(
          browser_view_->browser()->profile()->GetPath());
      new_avatar_button_ = new NewAvatarButton(
          listener, profile_name, style, browser_view_->browser());
      new_avatar_button_->set_id(VIEW_ID_NEW_AVATAR_BUTTON);
      AddChildView(new_avatar_button_);
      frame_->GetRootView()->Layout();
    }
  } else if (new_avatar_button_) {
    delete new_avatar_button_;
    new_avatar_button_ = NULL;
    frame_->GetRootView()->Layout();
  }
}
