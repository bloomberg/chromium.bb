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
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/ui/views/profiles/supervised_user_avatar_label.h"
#endif

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view),
      avatar_button_(nullptr),
#if defined(ENABLE_SUPERVISED_USERS)
      supervised_user_avatar_label_(nullptr),
#endif
      new_avatar_button_(nullptr) {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    cache.AddObserver(this);
  }
}

BrowserNonClientFrameView::~BrowserNonClientFrameView() {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    cache.RemoveObserver(this);
  }
}

void BrowserNonClientFrameView::UpdateToolbar() {
}

views::View* BrowserNonClientFrameView::GetLocationIconView() const {
  return nullptr;
}

void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  if (!is_visible)
    return;

  // The first time UpdateAvatarInfo() is called the window is not visible so
  // DrawTaskBarDecoration() has no effect. Therefore we need to call it again
  // once the window is visible.
  if (!browser_view_->IsRegularOrGuestSession() ||
      !switches::IsNewAvatarMenu()) {
    UpdateAvatarInfo();
  }

  // Make sure the task bar icon is correctly updated call
  // |OnProfileAvatarChanged()| in this case, but only for non guest profiles.
  if (!browser_view_->IsGuestSession() || !switches::IsNewAvatarMenu())
    OnProfileAvatarChanged(base::FilePath());
}

void BrowserNonClientFrameView::ChildPreferredSizeChanged(View* child) {
  // Only perform a re-layout if the avatar button has changed, since that
  // can affect the size of the tabs.
  if (child == new_avatar_button_) {
    InvalidateLayout();
    frame_->GetRootView()->Layout();
  }
}

#if defined(ENABLE_SUPERVISED_USERS)
void BrowserNonClientFrameView::OnThemeChanged() {
  if (supervised_user_avatar_label_)
    supervised_user_avatar_label_->UpdateLabelStyle();
}
#endif

void BrowserNonClientFrameView::UpdateAvatar() {
  if (browser_view()->IsRegularOrGuestSession() && switches::IsNewAvatarMenu())
    UpdateNewStyleAvatar();
  else
    UpdateAvatarInfo();
}

void BrowserNonClientFrameView::UpdateAvatarInfo() {
  if (browser_view_->ShouldShowAvatar()) {
    if (!avatar_button_) {
#if defined(ENABLE_SUPERVISED_USERS)
      Profile* profile = browser_view_->browser()->profile();
      if (profile->IsSupervised() && !supervised_user_avatar_label_) {
        supervised_user_avatar_label_ =
            new SupervisedUserAvatarLabel(browser_view_);
        supervised_user_avatar_label_->set_id(
            VIEW_ID_SUPERVISED_USER_AVATAR_LABEL);
        AddChildView(supervised_user_avatar_label_);
      }
#endif
      avatar_button_ = new AvatarMenuButton(
          browser_view_->browser(), !browser_view_->IsRegularOrGuestSession());
      avatar_button_->set_id(VIEW_ID_AVATAR_BUTTON);
      AddChildView(avatar_button_);
      // Invalidate here because adding a child does not invalidate the layout.
      InvalidateLayout();
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_) {
#if defined(ENABLE_SUPERVISED_USERS)
    // The avatar label can just be there if there is also an avatar button.
    if (supervised_user_avatar_label_) {
      RemoveChildView(supervised_user_avatar_label_);
      delete supervised_user_avatar_label_;
      supervised_user_avatar_label_ = nullptr;
    }
#endif
    RemoveChildView(avatar_button_);
    delete avatar_button_;
    avatar_button_ = nullptr;
    frame_->GetRootView()->Layout();
  }

  gfx::Image avatar;
  gfx::Image taskbar_badge_avatar;
  bool is_rectangle = false;

  // Update the avatar button in the window frame and the taskbar overlay.
  bool should_show_avatar_menu =
      avatar_button_ || AvatarMenu::ShouldShowAvatarMenu();

  if (!AvatarMenuButton::GetAvatarImages(
          browser_view_->browser()->profile(), should_show_avatar_menu, &avatar,
          &taskbar_badge_avatar, &is_rectangle)) {
    return;
  }

  // Disable the menu when we should not show the menu.
  if (avatar_button_ && !AvatarMenu::ShouldShowAvatarMenu())
    avatar_button_->SetEnabled(false);
  if (avatar_button_)
    avatar_button_->SetAvatarIcon(avatar, is_rectangle);
}

void BrowserNonClientFrameView::UpdateNewStyleAvatarInfo(
    views::ButtonListener* listener,
    const NewAvatarButton::AvatarButtonStyle style) {
  DCHECK(switches::IsNewAvatarMenu());
  // This should never be called in incognito mode.
  DCHECK(browser_view_->IsRegularOrGuestSession());

  if (browser_view_->ShouldShowAvatar()) {
    if (!new_avatar_button_) {
      new_avatar_button_ =
          new NewAvatarButton(listener, style, browser_view_->browser());
      new_avatar_button_->set_id(VIEW_ID_NEW_AVATAR_BUTTON);
      AddChildView(new_avatar_button_);
      frame_->GetRootView()->Layout();
    }
  } else if (new_avatar_button_) {
    delete new_avatar_button_;
    new_avatar_button_ = nullptr;
    frame_->GetRootView()->Layout();
  }
}

void BrowserNonClientFrameView::OnProfileAdded(
    const base::FilePath& profile_path) {
  UpdateTaskbarDecoration();
  UpdateAvatar();
}

void BrowserNonClientFrameView::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  UpdateTaskbarDecoration();
  UpdateAvatar();
}

void BrowserNonClientFrameView::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateTaskbarDecoration();
  // Profile avatars are only displayed in the old UI or incognito.
  if (browser_view()->IsOffTheRecord() || !switches::IsNewAvatarMenu())
    UpdateAvatarInfo();
}

void BrowserNonClientFrameView::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  UpdateAvatar();
}

void BrowserNonClientFrameView::UpdateTaskbarDecoration() {
  gfx::Image avatar;
  gfx::Image taskbar_badge_avatar;
  bool is_rectangle;
  // Only need to update the taskbar overlay here.  If GetAvatarImages()
  // returns false, don't bother trying to update the taskbar decoration since
  // the returned images are not initialized.  This can happen if the user
  // deletes the current profile.
  if (AvatarMenuButton::GetAvatarImages(browser_view_->browser()->profile(),
                                        AvatarMenu::ShouldShowAvatarMenu(),
                                        &avatar, &taskbar_badge_avatar,
                                        &is_rectangle)) {
    // For popups and panels which don't have the avatar button, we still
    // need to draw the taskbar decoration. Even though we have an icon on the
    // window's relaunch details, we draw over it because the user may have
    // pinned the badge-less Chrome shortcut which will cause windows to ignore
    // the relaunch details.
    // TODO(calamity): ideally this should not be necessary but due to issues
    // with the default shortcut being pinned, we add the runtime badge for
    // safety. See crbug.com/313800.
    bool show_decoration = AvatarMenu::ShouldShowAvatarMenu() &&
        !browser_view_->browser()->profile()->IsGuestSession();
    // In tests, make sure that the browser process and profile manager are
    // valid before using.
    if (g_browser_process && g_browser_process->profile_manager()) {
      const ProfileInfoCache& cache =
          g_browser_process->profile_manager()->GetProfileInfoCache();
      show_decoration = show_decoration && cache.GetNumberOfProfiles() > 1;
    }
    chrome::DrawTaskbarDecoration(frame_->GetNativeWindow(),
        show_decoration
            ? (taskbar_badge_avatar.IsEmpty() ? &avatar : &taskbar_badge_avatar)
            : nullptr);
  }
}
