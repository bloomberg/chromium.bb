// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"

AvatarToolbarButton::AvatarToolbarButton(Profile* profile,
                                         views::ButtonListener* listener)
    : ToolbarButton(profile, listener, nullptr),
      profile_(profile),
      error_controller_(this, profile_),
      profile_observer_(this) {
  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  SetImageAlignment(HorizontalAlignment::ALIGN_CENTER,
                    VerticalAlignment::ALIGN_MIDDLE);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                              ui::EF_MIDDLE_MOUSE_BUTTON);
  set_tag(IDC_SHOW_AVATAR_MENU);
  if (profile->IsOffTheRecord()) {
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_INCOGNITO_AVATAR_BUTTON_TOOLTIP));
    SetEnabled(false);
  } else {
    // TODO(pbos): Incorporate GetAvatarButtonTextForProfile. See
    // AvatarButton.
    SetTooltipText(l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL));
  }
  set_id(VIEW_ID_AVATAR_BUTTON);
  Init();
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  const bool is_touch =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled();
  // TODO(pbos): Move these constants to LayoutProvider or LayoutConstants.
  const int icon_size = is_touch ? 24 : 20;

  gfx::Image avatar_icon;
  // TODO(pbos): Account for incognito by either changing the icon and
  // effectively disabling the menu or by not showing it at all in incognito.
  if (AvatarMenu::GetImageForMenuButton(profile_->GetPath(), &avatar_icon) ==
      AvatarMenu::ImageLoadStatus::LOADED) {
    avatar_icon = profiles::GetSizedAvatarIcon(
        avatar_icon, true, icon_size, icon_size, profiles::SHAPE_CIRCLE);
    SetImage(views::Button::STATE_NORMAL, avatar_icon.ToImageSkia());
  } else {
    SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size,
                              GetThemeProvider()->GetColor(
                                  ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON)));
  }
}

void AvatarToolbarButton::OnAvatarErrorChanged() {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateIcon();
}
