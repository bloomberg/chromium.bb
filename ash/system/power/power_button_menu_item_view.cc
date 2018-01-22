// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_item_view.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Color of the title of the label.
constexpr SkColor kItemTitleColor = SkColorSetARGBMacro(0xFF, 0x5F, 0x63, 0x68);

// Font size of the title should be 14p. The default font size is 12p and
// MediumFont is 15p. We use MediumFont for the title plus the delta here.
constexpr int kTitleFontSizeDelta = -1;

// Size of the image icon in pixels.
constexpr int kIconSize = 24;

// Top padding of the image icon to the top of the item view.
constexpr int kIconTopPadding = 16;

// Top padding of the label of title to the top of the item view.
constexpr int kTitleTopPadding = 50;

}  // namespace

PowerButtonMenuItemView::PowerButtonMenuItemView(
    views::ButtonListener* listener,
    const gfx::VectorIcon& icon,
    const base::string16& title_text)
    : views::ImageButton(listener),
      icon_view_(new views::ImageView),
      title_(new views::Label) {
  SetPaintToLayer();
  icon_view_->SetImage(gfx::CreateVectorIcon(icon, gfx::kChromeIconGrey));
  AddChildView(icon_view_);

  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_->SetAutoColorReadabilityEnabled(false);

  title_->SetFontList(ui::ResourceBundle::GetSharedInstance()
                          .GetFontList(ui::ResourceBundle::MediumFont)
                          .DeriveWithSizeDelta(kTitleFontSizeDelta));
  title_->SetEnabledColor(kItemTitleColor);
  title_->SetText(title_text);
  AddChildView(title_);
}

PowerButtonMenuItemView::~PowerButtonMenuItemView() = default;

void PowerButtonMenuItemView::Layout() {
  const gfx::Rect rect(GetContentsBounds());

  gfx::Rect icon_rect(rect);
  icon_rect.ClampToCenteredSize(gfx::Size(kIconSize, kIconSize));
  icon_rect.set_y(kIconTopPadding);
  icon_view_->SetBoundsRect(icon_rect);

  gfx::Rect title_rect(rect);
  title_rect.ClampToCenteredSize(
      gfx::Size(kMenuItemWidth, title_->font_list().GetHeight()));
  title_rect.set_y(kTitleTopPadding);
  title_->SetBoundsRect(title_rect);
}

gfx::Size PowerButtonMenuItemView::CalculatePreferredSize() const {
  return gfx::Size(kMenuItemWidth, kMenuItemHeight);
}

void PowerButtonMenuItemView::PaintButtonContents(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  canvas->DrawColor(SK_ColorWHITE);
}

}  // namespace ash
