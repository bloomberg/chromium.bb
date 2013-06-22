// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_label.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/painter.h"

namespace {

// A special text button border for the managed user avatar label.
class AvatarLabelBorder: public views::TextButtonBorder {
 public:
  explicit AvatarLabelBorder(ui::ThemeProvider* theme_provider);

  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;

 private:
  scoped_ptr<views::Painter> hot_painter_;
  scoped_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(AvatarLabelBorder);
};

AvatarLabelBorder::AvatarLabelBorder(ui::ThemeProvider* theme_provider) {
  const int kHorizontalInset = 10;
  const int kVerticalInset = 2;
  SetInsets(gfx::Insets(
      kVerticalInset, kHorizontalInset, kVerticalInset, kHorizontalInset));
  SkColor color = theme_provider->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL_BACKGROUND);
  SkColor color2 = color_utils::BlendTowardOppositeLuminance(color, 0x20);
  painter_.reset(views::Painter::CreateVerticalGradient(color, color2));
  hot_painter_.reset(views::Painter::CreateVerticalGradient(color2, color));
}

void AvatarLabelBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  const views::TextButton* button =
      static_cast<const views::TextButton*>(&view);
  if (button->state() == views::TextButton::STATE_HOVERED ||
      button->state() == views::TextButton::STATE_PRESSED)
    hot_painter_->Paint(canvas, view.size());
  else
    painter_->Paint(canvas, view.size());
}

}  // namespace

AvatarLabel::AvatarLabel(BrowserView* browser_view,
                         ui::ThemeProvider* theme_provider)
    : TextButton(NULL,
                 l10n_util::GetStringUTF16(IDS_MANAGED_USER_AVATAR_LABEL)),
      browser_view_(browser_view),
      theme_provider_(theme_provider) {
  SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallFont));
  ClearMaxTextSize();
  set_border(new AvatarLabelBorder(theme_provider));
  UpdateLabelStyle();
}

AvatarLabel::~AvatarLabel() {}

bool AvatarLabel::OnMousePressed(const ui::MouseEvent& event) {
  if (!TextButton::OnMousePressed(event))
    return false;

  browser_view_->ShowAvatarBubbleFromAvatarButton();
  return true;
}

void AvatarLabel::UpdateLabelStyle() {
  SkColor color_label =
      theme_provider_->GetColor(ThemeProperties::COLOR_MANAGED_USER_LABEL);
  SetEnabledColor(color_label);
  SetHighlightColor(color_label);
  SetHoverColor(color_label);
  SetDisabledColor(color_label);
  SchedulePaint();
}
