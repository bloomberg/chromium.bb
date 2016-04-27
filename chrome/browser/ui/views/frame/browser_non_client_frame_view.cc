// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
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
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/resources/grit/views_resources.h"

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view) {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->
        GetProfileAttributesStorage().AddObserver(this);
  }
}

BrowserNonClientFrameView::~BrowserNonClientFrameView() {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->
        GetProfileAttributesStorage().RemoveObserver(this);
  }
}

void BrowserNonClientFrameView::OnBrowserViewInitViewsComplete() {}

gfx::ImageSkia BrowserNonClientFrameView::GetOTRAvatarIcon() const {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return *GetThemeProviderForProfile()->GetImageSkiaNamed(IDR_OTR_ICON);
  const SkColor icon_color = color_utils::PickContrastingColor(
      SK_ColorWHITE, gfx::kChromeIconGrey, GetFrameColor());
  return gfx::CreateVectorIcon(gfx::VectorIconId::INCOGNITO, icon_color);
}

SkColor BrowserNonClientFrameView::GetToolbarTopSeparatorColor() const {
  const auto color_id =
      ShouldPaintAsActive()
          ? ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR
          : ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE;
  return ShouldPaintAsThemed() ? GetThemeProvider()->GetColor(color_id)
                               : ThemeProperties::GetDefaultColor(
                                     color_id, browser_view_->IsOffTheRecord());
}

void BrowserNonClientFrameView::UpdateToolbar() {
}

views::View* BrowserNonClientFrameView::GetLocationIconView() const {
  return nullptr;
}

views::View* BrowserNonClientFrameView::GetProfileSwitcherView() const {
  return nullptr;
}

void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  // UpdateTaskbarDecoration() calls DrawTaskbarDecoration(), but that does
  // nothing if the window is not visible.  So even if we've already gotten the
  // up-to-date decoration, we need to run the update procedure again here when
  // the window becomes visible.
  if (is_visible)
    OnProfileAvatarChanged(base::FilePath());
}

bool BrowserNonClientFrameView::ShouldPaintAsThemed() const {
  return browser_view_->IsBrowserTypeNormal();
}

SkColor BrowserNonClientFrameView::GetFrameColor(bool active) const {
  ThemeProperties::OverwritableByUserThemeProperty color_id =
      active ? ThemeProperties::COLOR_FRAME
             : ThemeProperties::COLOR_FRAME_INACTIVE;
  return ShouldPaintAsThemed() ?
      GetThemeProviderForProfile()->GetColor(color_id) :
      ThemeProperties::GetDefaultColor(color_id,
                                       browser_view_->IsOffTheRecord());
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameImage(bool active) const {
  const ui::ThemeProvider* tp = frame_->GetThemeProvider();
  int frame_image_id = active ? IDR_THEME_FRAME : IDR_THEME_FRAME_INACTIVE;

  // |default_uses_color| means the default frame is painted with a solid color.
  // When false, the default frame is painted with assets.
#if defined(OS_CHROMEOS)
  bool default_uses_color = true;
#else
  bool default_uses_color = ui::MaterialDesignController::IsModeMaterial();
#endif
  if (default_uses_color) {
    return ShouldPaintAsThemed() && (tp->HasCustomImage(frame_image_id) ||
                                     tp->HasCustomImage(IDR_THEME_FRAME))
               ? *tp->GetImageSkiaNamed(frame_image_id)
               : gfx::ImageSkia();
  }

  return ShouldPaintAsThemed()
             ? *tp->GetImageSkiaNamed(frame_image_id)
             : *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                   frame_image_id);
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameOverlayImage(
    bool active) const {
  if (browser_view_->IsOffTheRecord() || !browser_view_->IsBrowserTypeNormal())
    return gfx::ImageSkia();

  const ui::ThemeProvider* tp = frame_->GetThemeProvider();
  int frame_overlay_image_id =
      active ? IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  return tp->HasCustomImage(frame_overlay_image_id)
             ? *tp->GetImageSkiaNamed(frame_overlay_image_id)
             : gfx::ImageSkia();
}

SkColor BrowserNonClientFrameView::GetFrameColor() const {
  return GetFrameColor(ShouldPaintAsActive());
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameImage() const {
  return GetFrameImage(ShouldPaintAsActive());
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameOverlayImage() const {
  return GetFrameOverlayImage(ShouldPaintAsActive());
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

  if (!AvatarMenuButton::GetAvatarImages(this, should_show_avatar_menu, &avatar,
                                         &taskbar_badge_avatar, &is_rectangle))
    return;

  // Disable the menu when we should not show the menu.
  if (avatar_button_ && !AvatarMenu::ShouldShowAvatarMenu())
    avatar_button_->SetEnabled(false);
  if (avatar_button_)
    avatar_button_->SetAvatarIcon(avatar, is_rectangle);
}

void BrowserNonClientFrameView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    UpdateAvatar();
}

void BrowserNonClientFrameView::ActivationChanged(bool active) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // On Windows, while deactivating the widget, this is called before the
    // active HWND has actually been changed.  Since we want the avatar state to
    // reflect that the window is inactive, we force NonClientFrameView to see
    // the "correct" state as an override.
    set_active_state_override(&active);
    UpdateAvatar();
    set_active_state_override(nullptr);

    // Changing the activation state may change the toolbar top separator color
    // that's used as the stroke around tabs/the new tab button.
    browser_view_->tabstrip()->SchedulePaint();
  }

  // Changing the activation state may change the visible frame color.
  SchedulePaint();
}

void BrowserNonClientFrameView::OnProfileAdded(
    const base::FilePath& profile_path) {
  OnProfileAvatarChanged(profile_path);
}

void BrowserNonClientFrameView::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  OnProfileAvatarChanged(profile_path);
}

void BrowserNonClientFrameView::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateTaskbarDecoration();
  UpdateAvatar();
}

const ui::ThemeProvider*
BrowserNonClientFrameView::GetThemeProviderForProfile() const {
  // Because the frame's accessor reads the ThemeProvider from the profile and
  // not the widget, it can be called even before we're in a view hierarchy.
  return frame_->GetThemeProvider();
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
          this, AvatarMenu::ShouldShowAvatarMenu(), &avatar,
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
      const ProfileAttributesStorage& storage =
          g_browser_process->profile_manager()->GetProfileAttributesStorage();
      show_decoration = show_decoration && storage.GetNumberOfProfiles() > 1;
    }
    chrome::DrawTaskbarDecoration(frame_->GetNativeWindow(),
        show_decoration
            ? (taskbar_badge_avatar.IsEmpty() ? &avatar : &taskbar_badge_avatar)
            : nullptr);
  }
}
