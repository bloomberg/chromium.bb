// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/taskbar_decorator.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/resources/grit/views_resources.h"

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view),
#if defined(FRAME_AVATAR_BUTTON)
      profile_switcher_(this),
#endif
      avatar_button_(nullptr) {
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

void BrowserNonClientFrameView::OnBrowserViewInitViewsComplete() {}

void BrowserNonClientFrameView::UpdateToolbar() {
}

views::View* BrowserNonClientFrameView::GetLocationIconView() const {
  return nullptr;
}

void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  if (!is_visible)
    return;

#if defined(OS_CHROMEOS)
  // On ChromeOS we always need to give the old avatar button a chance to update
  // in case we're in a teleported window. On desktop, the old avatar button
  // only shows up when in incognito mode.
  UpdateOldAvatarButton();
  OnProfileAvatarChanged(base::FilePath());
#else
  if (!browser_view_->IsRegularOrGuestSession()) {
    // The first time UpdateOldAvatarButton() is called the window is not
    // visible so DrawTaskBarDecoration() has no effect. Therefore we need to
    // call it again once the window is visible.
    UpdateOldAvatarButton();
  }

  // Call OnProfileAvatarChanged() in this case to make sure the task bar icon
  // is correctly updated. Guest profiles don't badge the icon so no need to do
  // this in guest mode.
  if (!browser_view_->IsGuestSession())
    OnProfileAvatarChanged(base::FilePath());
#endif
}

void BrowserNonClientFrameView::ChildPreferredSizeChanged(View* child) {
#if defined(FRAME_AVATAR_BUTTON)
  // Only perform a re-layout if the avatar button has changed, since that
  // can affect the size of the tabs.
  if (child == new_avatar_button()) {
    InvalidateLayout();
    frame_->GetRootView()->Layout();
  }
#endif
}

bool BrowserNonClientFrameView::ShouldPaintAsThemed() const {
  return browser_view_->IsBrowserTypeNormal();
}

#if !defined(OS_CHROMEOS)
SkColor BrowserNonClientFrameView::GetFrameColor() const {
  ThemeProperties::OverwritableByUserThemeProperty color_id =
      ShouldPaintAsActive() ? ThemeProperties::COLOR_FRAME
                            : ThemeProperties::COLOR_FRAME_INACTIVE;
  return ShouldPaintAsThemed() ? GetThemeProvider()->GetColor(color_id)
                               : ThemeProperties::GetDefaultColor(
                                     color_id, browser_view_->IsOffTheRecord());
}

gfx::ImageSkia* BrowserNonClientFrameView::GetFrameImage() const {
  const bool incognito = browser_view_->IsOffTheRecord();
  int resource_id;
  if (browser_view_->IsBrowserTypeNormal()) {
    if (ShouldPaintAsActive()) {
      resource_id = incognito ? IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
    } else {
      resource_id = incognito ? IDR_THEME_FRAME_INCOGNITO_INACTIVE
                              : IDR_THEME_FRAME_INACTIVE;
    }
    return GetThemeProvider()->GetImageSkiaNamed(resource_id);
  }

  if (ShouldPaintAsActive()) {
    resource_id = incognito ? IDR_THEME_FRAME_INCOGNITO : IDR_FRAME;
  } else {
    resource_id = incognito ? IDR_THEME_FRAME_INCOGNITO_INACTIVE
                            : IDR_THEME_FRAME_INACTIVE;
  }

  if (ShouldPaintAsThemed()) {
    // On Linux, we want to use theme images provided by the system theme when
    // enabled, even if we are an app or popup window.
    return GetThemeProvider()->GetImageSkiaNamed(resource_id);
  }

  // Otherwise, never theme app and popup windows.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(resource_id);
}

gfx::ImageSkia* BrowserNonClientFrameView::GetFrameOverlayImage() const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view_->IsBrowserTypeNormal() &&
      !browser_view_->IsOffTheRecord()) {
    return tp->GetImageSkiaNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE);
  }
  return nullptr;
}
#endif  // !defined(OS_CHROMEOS)

void BrowserNonClientFrameView::UpdateAvatar() {
#if !defined(OS_CHROMEOS)
  if (browser_view()->IsRegularOrGuestSession())
    UpdateNewAvatarButtonImpl();
  else
#endif
    UpdateOldAvatarButton();
}

void BrowserNonClientFrameView::UpdateOldAvatarButton() {
  if (browser_view_->ShouldShowAvatar()) {
    if (!avatar_button_) {
      avatar_button_ = new AvatarMenuButton(browser_view_);
      avatar_button_->set_id(VIEW_ID_AVATAR_BUTTON);
      AddChildView(avatar_button_);
      // Invalidate here because adding a child does not invalidate the layout.
      InvalidateLayout();
      frame_->GetRootView()->Layout();
    }
  } else if (avatar_button_) {
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

  if (!AvatarMenuButton::GetAvatarImages(browser_view_, should_show_avatar_menu,
                                         &avatar, &taskbar_badge_avatar,
                                         &is_rectangle)) {
    return;
  }

  // Disable the menu when we should not show the menu.
  if (avatar_button_ && !AvatarMenu::ShouldShowAvatarMenu())
    avatar_button_->SetEnabled(false);
  if (avatar_button_)
    avatar_button_->SetAvatarIcon(avatar, is_rectangle);
}

#if defined(FRAME_AVATAR_BUTTON)
void BrowserNonClientFrameView::UpdateNewAvatarButton(
    const AvatarButtonStyle style) {
  profile_switcher_.Update(style);
}
#endif

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
  // Profile avatars are only displayed in incognito or on ChromeOS teleported
  // windows.
#if !defined(OS_CHROMEOS)
  if (!browser_view()->IsGuestSession() && browser_view()->IsOffTheRecord())
#endif
    UpdateOldAvatarButton();
}

void BrowserNonClientFrameView::UpdateTaskbarDecoration() {
  gfx::Image avatar;
  gfx::Image taskbar_badge_avatar;
  bool is_rectangle;
  // Only need to update the taskbar overlay here.  If GetAvatarImages()
  // returns false, don't bother trying to update the taskbar decoration since
  // the returned images are not initialized.  This can happen if the user
  // deletes the current profile.
  if (AvatarMenuButton::GetAvatarImages(
          browser_view_, AvatarMenu::ShouldShowAvatarMenu(), &avatar,
          &taskbar_badge_avatar, &is_rectangle)) {
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
