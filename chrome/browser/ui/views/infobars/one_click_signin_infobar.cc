// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/one_click_signin_infobar.h"

#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/sync/one_click_signin_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/text_button.h"

namespace {

// Preferred padding between text and edge.
const int kPreferredPaddingHorizontal = 6;

// A border used for infobar buttons using a custom colour.
class InfoBarColoredButtonBorder : public views::Border {
 public:
  InfoBarColoredButtonBorder(SkColor background_color,
                             SkColor border_color);

 private:
  // A helper function to easily perform colour darkening from the
  // constructor initializer list.
  static SkColor DarkenColor(SkColor color);

  // Border overrides:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;

  virtual ~InfoBarColoredButtonBorder();

  const SkColor background_color_;
  const SkColor border_color_;
  const SkColor border_color_hot_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarColoredButtonBorder);
};

InfoBarColoredButtonBorder::InfoBarColoredButtonBorder(
    SkColor background_color,
    SkColor border_color)
  : background_color_(background_color),
    border_color_(border_color),
    border_color_hot_(DarkenColor(border_color_)) {
}

// static
SkColor InfoBarColoredButtonBorder::DarkenColor(SkColor color) {
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);
  hsv[2] *= 0.8f;
  return SkHSVToColor(255, hsv);
}

void InfoBarColoredButtonBorder::Paint(const views::View& view,
                                       gfx::Canvas* canvas) {
  const views::CustomButton* button =
      static_cast<const views::CustomButton*>(&view);
  const views::CustomButton::ButtonState state = button->state();

  const SkScalar kRadius = 2.0;

  SkRect bounds(gfx::RectToSkRect(view.GetLocalBounds()));
  bounds.inset(0.5, 0.5);

  SkPaint paint;
  paint.setAntiAlias(true);

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(background_color_);
  canvas->sk_canvas()->drawRoundRect(bounds, kRadius, kRadius, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(state == views::CustomButton::STATE_NORMAL ?
      border_color_ : border_color_hot_);
  canvas->sk_canvas()->drawRoundRect(bounds, kRadius, kRadius, paint);
}

gfx::Insets InfoBarColoredButtonBorder::GetInsets() const {
  return gfx::Insets(browser_defaults::kInfoBarBorderPaddingVertical,
                     kPreferredPaddingHorizontal,
                     browser_defaults::kInfoBarBorderPaddingVertical,
                     kPreferredPaddingHorizontal);
}

InfoBarColoredButtonBorder::~InfoBarColoredButtonBorder() {
}

}  // namespace


// OneClickSigninInfoBarDelegate ----------------------------------------------

InfoBar* OneClickSigninInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new OneClickSigninInfoBar(owner, this);
}


// OneClickLoginInfoBar -------------------------------------------------------

OneClickSigninInfoBar::OneClickSigninInfoBar(
    InfoBarService* owner,
    OneClickSigninInfoBarDelegate* delegate)
  : ConfirmInfoBar(owner, delegate),
    one_click_delegate_(delegate) {
  CHECK(one_click_delegate_);
}

OneClickSigninInfoBar::~OneClickSigninInfoBar() {
}

void OneClickSigninInfoBar::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  const bool fix_color = is_add && child == this && ok_button() == NULL;

  ConfirmInfoBar::ViewHierarchyChanged(is_add, parent, child);

  if (!fix_color || ok_button() == NULL)
    return;

  OneClickSigninInfoBarDelegate::AlternateColors alt_colors;
  one_click_delegate_->GetAlternateColors(&alt_colors);
  if (!alt_colors.enabled)
    return;

  set_background(new InfoBarBackground(alt_colors.infobar_top_color,
                                       alt_colors.infobar_bottom_color));
  ok_button()->set_border(new InfoBarColoredButtonBorder(
      alt_colors.button_background_color,
      alt_colors.button_border_color));
  ok_button()->SetEnabledColor(alt_colors.button_text_color);
  ok_button()->SetHighlightColor(alt_colors.button_text_color);
  ok_button()->SetHoverColor(alt_colors.button_text_color);
}
