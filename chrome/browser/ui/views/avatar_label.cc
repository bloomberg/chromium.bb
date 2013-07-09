// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_label.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
  scoped_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(AvatarLabelBorder);
};

AvatarLabelBorder::AvatarLabelBorder(ui::ThemeProvider* theme_provider) {
  const int kHorizontalInsetRight = 10;
  const int kHorizontalInsetLeft = 43;
  const int kVerticalInsetTop = 3;
  const int kVerticalInsetBottom = 3;
  // We want to align with the top of the tab. This works if the BaseFont size
  // is 13. If it is smaller, we need to increase the TopInset accordingly.
  gfx::Font font = ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
  int difference = (font.GetFontSize() < 13) ? 13 - font.GetFontSize() : 0;
  int addToTop = difference / 2;
  int addToBottom = difference - addToTop;
  SetInsets(gfx::Insets(kVerticalInsetTop + addToTop,
                        kHorizontalInsetLeft,
                        kVerticalInsetBottom + addToBottom,
                        kHorizontalInsetRight));
  const int kImages[] = IMAGE_GRID(IDR_MANAGED_USER_LABEL);
  painter_.reset(views::Painter::CreateImageGridPainter(kImages));
}

void AvatarLabelBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
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
      ui::ResourceBundle::BaseFont));
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
