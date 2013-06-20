// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_label.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

AvatarLabel::AvatarLabel(BrowserView* browser_view,
                         ui::ThemeProvider* theme_provider)
    : TextButton(NULL,
                 l10n_util::GetStringUTF16(IDS_MANAGED_USER_AVATAR_LABEL)),
      browser_view_(browser_view),
      theme_provider_(theme_provider) {
  SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallFont));
  ClearMaxTextSize();
  views::TextButtonNativeThemeBorder* border =
      new views::TextButtonNativeThemeBorder(this);
  const int kHorizontalInset = 10;
  const int kVerticalInset = 2;
  border->SetInsets(gfx::Insets(
      kVerticalInset, kHorizontalInset, kVerticalInset, kHorizontalInset));
  set_border(border);
  UpdateLabelStyle();
}

AvatarLabel::~AvatarLabel() {}

bool AvatarLabel::OnMousePressed(const ui::MouseEvent& event) {
  if (!TextButton::OnMousePressed(event))
    return false;

  browser_view_->ShowAvatarBubbleFromAvatarButton();
  return true;
}

void AvatarLabel::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  TextButton::GetExtraParams(params);
  params->button.background_color = theme_provider_->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL_BACKGROUND);
}

void AvatarLabel::UpdateLabelStyle() {
  SkColor color_label =
      theme_provider_->GetColor(ThemeProperties::COLOR_MANAGED_USER_LABEL);
  SetEnabledColor(color_label);
  SchedulePaint();
}
