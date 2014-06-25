// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_label.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace {

// A custom border for the supervised user avatar label.
class AvatarLabelBorder : public views::Border {
 public:
  explicit AvatarLabelBorder(bool label_on_right);

  // views::Border:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  scoped_ptr<views::Painter> painter_;
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(AvatarLabelBorder);
};

AvatarLabelBorder::AvatarLabelBorder(bool label_on_right) {
  const int kHorizontalInsetRight = label_on_right ? 43 : 10;
  const int kHorizontalInsetLeft = label_on_right ? 10 : 43;
  const int kVerticalInsetTop = 2;
  const int kVerticalInsetBottom = 3;
  // We want to align with the top of the tab. This works if the default font
  // size is 13. If it is smaller, we need to increase the TopInset accordingly.
  const int difference = std::max<int>(0, 13 - gfx::FontList().GetFontSize());
  const int addToTop = difference / 2;
  const int addToBottom = difference - addToTop;
  insets_ = gfx::Insets(kVerticalInsetTop + addToTop,
                        kHorizontalInsetLeft,
                        kVerticalInsetBottom + addToBottom,
                        kHorizontalInsetRight);
  const int kImages[] = IMAGE_GRID(IDR_SUPERVISED_USER_LABEL);
  painter_.reset(views::Painter::CreateImageGridPainter(kImages));
}

void AvatarLabelBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  // Paint the default background using the image assets provided by UI. This
  // includes a border with almost transparent white color.
  painter_->Paint(canvas, view.size());

  // Repaint the inner part of the background in order to be able to change
  // the colors according to the currently installed theme.
  gfx::Rect rect(1, 1, view.size().width() - 2, view.size().height() - 2);
  SkPaint paint;
  int kRadius = 2;
  SkColor background_color = view.GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BACKGROUND);
  paint.setStyle(SkPaint::kFill_Style);

  // Paint the inner border with a color slightly darker than the background.
  SkAlpha kAlphaForBlending = 230;
  paint.setColor(color_utils::AlphaBlend(
      background_color, SK_ColorBLACK, kAlphaForBlending));
  canvas->DrawRoundRect(rect, kRadius, paint);

  // Paint the inner background using the color provided by the ThemeProvider.
  paint.setColor(background_color);
  rect = gfx::Rect(2, 2, view.size().width() - 4, view.size().height() - 4);
  canvas->DrawRoundRect(rect, kRadius, paint);
}

gfx::Insets AvatarLabelBorder::GetInsets() const {
  return insets_;
}

gfx::Size AvatarLabelBorder::GetMinimumSize() const {
  gfx::Size size(4, 4);
  size.SetToMax(painter_->GetMinimumSize());
  return size;
}

}  // namespace

AvatarLabel::AvatarLabel(BrowserView* browser_view)
    : LabelButton(NULL,
                  l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_AVATAR_LABEL)),
      browser_view_(browser_view) {
  SetLabelOnRight(false);
  UpdateLabelStyle();
}

AvatarLabel::~AvatarLabel() {}

bool AvatarLabel::OnMousePressed(const ui::MouseEvent& event) {
  if (!LabelButton::OnMousePressed(event))
    return false;

  browser_view_->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT,
      signin::ManageAccountsParams());
  return true;
}

void AvatarLabel::UpdateLabelStyle() {
  // |browser_view_| can be NULL in unit tests.
  if (!browser_view_)
    return;

  SkColor color_label = browser_view_->frame()->GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_SUPERVISED_USER_LABEL);
  for (size_t state = 0; state < STATE_COUNT; ++state)
    SetTextColor(static_cast<ButtonState>(state), color_label);
  SchedulePaint();
}

void AvatarLabel::SetLabelOnRight(bool label_on_right) {
  SetBorder(scoped_ptr<views::Border>(new AvatarLabelBorder(label_on_right)));
}
