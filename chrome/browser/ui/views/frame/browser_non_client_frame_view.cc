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

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/taskbar_decorator_win.h"
#endif

BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view),
      profile_indicator_icon_(nullptr) {
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

void BrowserNonClientFrameView::UpdateProfileIndicatorIcon() {
  if (!profile_indicator_icon_) {
    profile_indicator_icon_ = new ProfileIndicatorIcon();
    profile_indicator_icon_->set_id(VIEW_ID_PROFILE_INDICATOR_ICON);
    AddChildView(profile_indicator_icon_);
    // Invalidate here because adding a child does not invalidate the layout.
    InvalidateLayout();
    frame_->GetRootView()->Layout();
  }

  gfx::Image icon;
  const Profile* profile = browser_view()->browser()->profile();
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    icon = gfx::Image(GetOTRAvatarIcon());
    if (!ui::MaterialDesignController::IsModeMaterial())
      profile_indicator_icon_->EnableCanvasFlippingForRTLUI(true);
  } else {
#if defined(OS_CHROMEOS)
    AvatarMenu::GetImageForMenuButton(profile->GetPath(), &icon);
#else
    NOTREACHED();
#endif
  }

  profile_indicator_icon_->SetIcon(icon);
}

void BrowserNonClientFrameView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    UpdateProfileIcons();
}

void BrowserNonClientFrameView::ActivationChanged(bool active) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // On Windows, while deactivating the widget, this is called before the
    // active HWND has actually been changed.  Since we want the avatar state to
    // reflect that the window is inactive, we force NonClientFrameView to see
    // the "correct" state as an override.
    set_active_state_override(&active);
    UpdateProfileIcons();
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
  UpdateProfileIcons();
}

const ui::ThemeProvider*
BrowserNonClientFrameView::GetThemeProviderForProfile() const {
  // Because the frame's accessor reads the ThemeProvider from the profile and
  // not the widget, it can be called even before we're in a view hierarchy.
  return frame_->GetThemeProvider();
}

void BrowserNonClientFrameView::UpdateTaskbarDecoration() {
#if defined(OS_WIN)
  if (browser_view()->browser()->profile()->IsGuestSession() ||
      // Browser process and profile manager may be null in tests.
      (g_browser_process && g_browser_process->profile_manager() &&
       g_browser_process->profile_manager()
               ->GetProfileAttributesStorage()
               .GetNumberOfProfiles() <= 1)) {
    chrome::DrawTaskbarDecoration(frame_->GetNativeWindow(), nullptr);
    return;
  }

  // For popups and panels which don't have the avatar button, we still
  // need to draw the taskbar decoration. Even though we have an icon on the
  // window's relaunch details, we draw over it because the user may have
  // pinned the badge-less Chrome shortcut which will cause Windows to ignore
  // the relaunch details.
  // TODO(calamity): ideally this should not be necessary but due to issues
  // with the default shortcut being pinned, we add the runtime badge for
  // safety. See crbug.com/313800.
  gfx::Image decoration;
  AvatarMenu::GetImageForMenuButton(
      browser_view()->browser()->profile()->GetPath(), &decoration);
  // This can happen if the user deletes the current profile.
  if (decoration.IsEmpty())
    return;
  chrome::DrawTaskbarDecoration(frame_->GetNativeWindow(), &decoration);
#endif
}
