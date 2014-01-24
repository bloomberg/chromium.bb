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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/views/painter.h"

namespace {

// A special text button border for the managed user avatar label.
class AvatarLabelBorder: public views::TextButtonBorder {
 public:
  explicit AvatarLabelBorder(bool label_on_right);

  // views::TextButtonBorder:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  scoped_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(AvatarLabelBorder);
};

AvatarLabelBorder::AvatarLabelBorder(bool label_on_right) {
  const int kHorizontalInsetRight = label_on_right ? 43 : 10;
  const int kHorizontalInsetLeft = label_on_right ? 10 : 43;
  const int kVerticalInsetTop = 2;
  const int kVerticalInsetBottom = 3;
  // We want to align with the top of the tab. This works if the default font
  // size is 13. If it is smaller, we need to increase the TopInset accordingly.
  const gfx::FontList font_list;
  int difference =
      (font_list.GetFontSize() < 13) ? 13 - font_list.GetFontSize() : 0;
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
  // Paint the default background using the image assets provided by UI. This
  // includes a border with almost transparent white color.
  painter_->Paint(canvas, view.size());

  // Now repaint the inner part of the background in order to be able to change
  // the colors according to the currently installed theme.
  gfx::Rect rect(1, 1, view.size().width() - 2, view.size().height() - 2);
  SkPaint paint;
  int kRadius = 2;
  SkColor background_color = view.GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL_BACKGROUND);
  paint.setStyle(SkPaint::kFill_Style);

  // For the inner border, use a color which is slightly darker than the
  // background color.
  SkAlpha kAlphaForBlending = 230;
  paint.setColor(color_utils::AlphaBlend(
      background_color, SK_ColorBLACK, kAlphaForBlending));
  canvas->DrawRoundRect(rect, kRadius, paint);

  // Now paint the inner background using the color provided by the
  // ThemeProvider.
  paint.setColor(background_color);
  rect = gfx::Rect(2, 2, view.size().width() - 4, view.size().height() - 4);
  canvas->DrawRoundRect(rect, kRadius, paint);
}

gfx::Size AvatarLabelBorder::GetMinimumSize() const {
  gfx::Size size(4, 4);
  size.SetToMax(painter_->GetMinimumSize());
  return size;
}

}  // namespace

AvatarLabel::AvatarLabel(BrowserView* browser_view)
    : TextButton(NULL,
                 l10n_util::GetStringUTF16(IDS_MANAGED_USER_AVATAR_LABEL)),
      browser_view_(browser_view) {
  ClearMaxTextSize();
  SetLabelOnRight(false);
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
  // |browser_view_| can be NULL in unit tests.
  if (!browser_view_)
    return;

  SkColor color_label = browser_view_->frame()->GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_MANAGED_USER_LABEL);
  SetEnabledColor(color_label);
  SetHighlightColor(color_label);
  SetHoverColor(color_label);
  SetDisabledColor(color_label);
  SchedulePaint();
}

void AvatarLabel::SetLabelOnRight(bool label_on_right) {
  SetBorder(scoped_ptr<views::Border>(new AvatarLabelBorder(label_on_right)));
}
