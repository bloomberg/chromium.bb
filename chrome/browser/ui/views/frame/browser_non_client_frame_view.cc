// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_properties.h"
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
#include "ui/views/controls/label.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view) {
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
  UpdateAvatarLabelStyle();
}

void BrowserNonClientFrameView::UpdateAvatarLabelStyle() {
  if (!avatar_label_.get())
    return;

  ui::ThemeProvider* tp = frame_->GetThemeProvider();
  SkColor color_background = tp->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL_BACKGROUND);
  avatar_label_->set_background(
      views::Background::CreateSolidBackground(color_background));
  avatar_label_->SetBackgroundColor(color_background);
  SkColor color_label = tp->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL);
  avatar_label_->SetEnabledColor(color_label);
}

void BrowserNonClientFrameView::UpdateAvatarInfo() {
  if (browser_view_->ShouldShowAvatar()) {
    if (!avatar_button_.get()) {
      avatar_button_.reset(
          new AvatarMenuButton(browser_view_->browser(),
                               browser_view_->IsOffTheRecord()));
      AddChildView(avatar_button_.get());
#if defined(ENABLE_MANAGED_USERS)
      Profile* profile = browser_view_->browser()->profile();
      ManagedUserService* service =
          ManagedUserServiceFactory::GetForProfile(profile);
      if (service->ProfileIsManaged() && !avatar_label_.get()) {
        ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
        avatar_label_.reset(new views::Label(
            l10n_util::GetStringUTF16(IDS_MANAGED_USER_AVATAR_LABEL),
            rb.GetFont(ui::ResourceBundle::BoldFont)));
        UpdateAvatarLabelStyle();
        AddChildView(avatar_label_.get());
      }
#endif
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_.get()) {
    // The avatar label can just be there if there is also an avatar button.
    if (avatar_label_.get()) {
      RemoveChildView(avatar_label_.get());
      avatar_label_.reset();
    }
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
  chrome::DrawTaskbarDecoration(
      frame_->GetNativeWindow(),
      AvatarMenuModel::ShouldShowAvatarMenu() ? &avatar : NULL);
}
