// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab.h"

#include "app/gfx/canvas.h"
#include "app/gfx/skia_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/string_util.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"

namespace {
const int kVerticalTabHeight = 27;
const int kIconSize = 16;
const int kIconTitleSpacing = 4;
const int kTitleCloseSpacing = 4;
const SkScalar kRoundRectRadius = 5;
const SkColor kTabBackgroundColor = SK_ColorWHITE;
const SkAlpha kBackgroundTabAlpha = 200;
static const int kHoverDurationMs = 900;
};

// static
gfx::Font* SideTab::font_ = NULL;
SkBitmap* SideTab::close_button_n_ = NULL;
SkBitmap* SideTab::close_button_h_ = NULL;
SkBitmap* SideTab::close_button_p_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// SideTab, public:

SideTab::SideTab(SideTabModel* model)
    : model_(model),
      close_button_(NULL) {
  InitClass();

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL, close_button_n_);
  close_button->SetImage(views::CustomButton::BS_HOT, close_button_h_);
  close_button->SetImage(views::CustomButton::BS_PUSHED, close_button_p_);
  close_button_ = close_button;
  AddChildView(close_button_);

  hover_animation_.reset(new SlideAnimation(this));
  hover_animation_->SetSlideDuration(kHoverDurationMs);
}

SideTab::~SideTab() {
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, AnimationDelegate implementation:

void SideTab::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void SideTab::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void SideTab::AnimationEnded(const Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, views::ButtonListener implementation:

void SideTab::ButtonPressed(views::Button* sender, const views::Event& event) {
  DCHECK(sender == close_button_);
  model_->CloseTab(this);
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, views::View overrides:

void SideTab::Layout() {
  int icon_y;
  int icon_x = icon_y = (height() - kIconSize) / 2;
  icon_bounds_.SetRect(icon_x, icon_y, kIconSize, kIconSize);

  gfx::Size ps = close_button_->GetPreferredSize();
  int close_y = (height() - ps.height()) / 2;
  close_button_->SetBounds(width() - ps.width() - close_y, close_y, ps.width(),
                           ps.height());

  int title_y = (height() - font_->height()) / 2;
  int title_x = icon_bounds_.right() + kIconTitleSpacing;
  title_bounds_.SetRect(title_x, title_y,
                        close_button_->x() - kTitleCloseSpacing - title_x,
                        font_->height());
}

void SideTab::Paint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setColor(kTabBackgroundColor);
  paint.setAntiAlias(true);
  SkRect background_rect = gfx::RectToSkRect(GetLocalBounds(false));
  canvas->drawRoundRect(background_rect, kRoundRectRadius, kRoundRectRadius,
                        paint);

  canvas->DrawBitmapInt(model_->GetIcon(this), 0, 0, kIconSize, kIconSize,
                        icon_bounds_.x(), icon_bounds_.y(),
                        icon_bounds_.width(), icon_bounds_.height(), false);
  canvas->DrawStringInt(UTF16ToWideHack(model_->GetTitle(this)), *font_,
                        SK_ColorBLACK, title_bounds_.x(), title_bounds_.y(),
                        title_bounds_.width(), title_bounds_.height());

  if (!model_->IsSelected(this) &&
      GetThemeProvider()->ShouldUseNativeFrame()) {
    // Make sure un-selected tabs are somewhat transparent.
    SkPaint paint;

    SkAlpha opacity = kBackgroundTabAlpha;
    if (hover_animation_->IsAnimating())
      opacity = static_cast<SkAlpha>(hover_animation_->GetCurrentValue() * 255);

    paint.setColor(SkColorSetARGB(kBackgroundTabAlpha, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    canvas->drawRoundRect(background_rect, kRoundRectRadius, kRoundRectRadius,
                          paint);
  }
}

gfx::Size SideTab::GetPreferredSize() {
  return gfx::Size(0, 27);
}

void SideTab::OnMouseEntered(const views::MouseEvent& event) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_OUT);
  hover_animation_->Show();
}

void SideTab::OnMouseExited(const views::MouseEvent& event) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_IN);
  hover_animation_->Hide();
}

bool SideTab::OnMousePressed(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    model_->SelectTab(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, private:

// static
void SideTab::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font_ = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));

    close_button_n_ = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    close_button_h_ = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    close_button_p_ = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);

    initialized = true;
  }
}

