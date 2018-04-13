// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"

AvatarToolbarButton::AvatarToolbarButton(Profile* profile,
                                         views::ButtonListener* listener)
    : ToolbarButton(profile, listener, nullptr) {
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

void AvatarToolbarButton::UpdateIcon() {
  const bool is_touch =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled();
  // TODO(pbos): Move these constants to LayoutProvider or LayoutConstants.
  const int icon_size = is_touch ? 24 : 16;

  // TODO(pbos): Account for incognito by either changing the icon and
  // effectively disabling the menu or by not showing it at all in incognito.
  SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size,
                            GetThemeProvider()->GetColor(
                                ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON)));
}
