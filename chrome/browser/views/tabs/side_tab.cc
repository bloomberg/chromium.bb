// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab.h"

#include "app/gfx/canvas.h"
#include "app/gfx/skia_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/utf_string_conversions.h"
#include "gfx/path.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"

namespace {
const int kVerticalTabHeight = 27;
const int kIconSize = 16;
const int kIconTitleSpacing = 4;
const int kTitleCloseSpacing = 4;
const SkScalar kRoundRectRadius = 5;
const SkColor kTabBackgroundColor = SK_ColorWHITE;
const SkAlpha kBackgroundTabAlpha = 170;
static const int kHoverDurationMs = 900;

static SkBitmap* waiting_animation_frames = NULL;
static SkBitmap* loading_animation_frames = NULL;
static int loading_animation_frame_count = 0;
static int waiting_animation_frame_count = 0;
static int waiting_to_loading_frame_count_ratio = 0;
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
      close_button_(NULL),
      current_state_(SideTabStripModel::NetworkState_None),
      loading_animation_frame_(0) {
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

void SideTab::SetNetworkState(SideTabStripModel::NetworkState state) {
  if (current_state_ != state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (current_state_ == SideTabStripModel::NetworkState_Waiting &&
        current_state_ == SideTabStripModel::NetworkState_Loading) {
      loading_animation_frame_ = loading_animation_frame_count -
          (loading_animation_frame_ / waiting_to_loading_frame_count_ratio);
    }
    current_state_ = state;
  }

  if (current_state_ != SideTabStripModel::NetworkState_None) {
    loading_animation_frame_ = ++loading_animation_frame_ %
        ((current_state_ == SideTabStripModel::NetworkState_Waiting) ?
            waiting_animation_frame_count :
            loading_animation_frame_count);
  } else {
    loading_animation_frame_ = 0;
  }
  SchedulePaint();
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
  gfx::Path tab_shape;
  FillTabShapePath(&tab_shape);
  canvas->drawPath(tab_shape, paint);

  PaintIcon(canvas);
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
    canvas->FillRectInt(0, 0, width(), height(), paint);
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

#define CUBIC_ARC_FACTOR    ((SK_ScalarSqrt2 - SK_Scalar1) * 4 / 3)

void SideTab::FillTabShapePath(gfx::Path* path) {
  SkScalar s = SkScalarMul(kRoundRectRadius, CUBIC_ARC_FACTOR);
  path->moveTo(SkIntToScalar(kRoundRectRadius), 0);
  path->cubicTo(SkIntToScalar(kRoundRectRadius) - s, 0, 0,
                SkIntToScalar(kRoundRectRadius) - s, 0,
                SkIntToScalar(kRoundRectRadius));
  path->lineTo(0, SkIntToScalar(height() - kRoundRectRadius));
  path->cubicTo(0, SkIntToScalar(height() - kRoundRectRadius) + s,
                SkIntToScalar(kRoundRectRadius) - s, SkIntToScalar(height()),
                SkIntToScalar(kRoundRectRadius),
                SkIntToScalar(height()));
  path->lineTo(SkIntToScalar(width()), SkIntToScalar(height()));
  path->lineTo(SkIntToScalar(width()), 0);
  path->lineTo(SkIntToScalar(kRoundRectRadius), 0);
  path->close();
}

void SideTab::PaintIcon(gfx::Canvas* canvas) {
  if (current_state_ == SideTabStripModel::NetworkState_None) {
    canvas->DrawBitmapInt(model_->GetIcon(this), 0, 0, kIconSize, kIconSize,
                          icon_bounds_.x(), icon_bounds_.y(),
                          icon_bounds_.width(), icon_bounds_.height(), false);
  } else {
    PaintLoadingAnimation(canvas);
  }
}

void SideTab::PaintLoadingAnimation(gfx::Canvas* canvas) {
  SkBitmap* frames =
      (current_state_ == SideTabStripModel::NetworkState_Waiting) ?
          waiting_animation_frames : loading_animation_frames;
  int image_size = frames->height();
  int image_offset = loading_animation_frame_ * image_size;
  int dst_y = (height() - image_size) / 2;
  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                        image_size, icon_bounds_.x(), dst_y, image_size,
                        image_size, false);
}

// static
void SideTab::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font_ = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));

    close_button_n_ = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    close_button_h_ = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    close_button_p_ = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);

    // The loading animation image is a strip of states. Each state must be
    // square, so the height must divide the width evenly.
    loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
    DCHECK(loading_animation_frames);
    DCHECK(loading_animation_frames->width() %
           loading_animation_frames->height() == 0);
    loading_animation_frame_count =
        loading_animation_frames->width() / loading_animation_frames->height();

    waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
    DCHECK(waiting_animation_frames);
    DCHECK(waiting_animation_frames->width() %
           waiting_animation_frames->height() == 0);
    waiting_animation_frame_count =
        waiting_animation_frames->width() / waiting_animation_frames->height();

    waiting_to_loading_frame_count_ratio =
        waiting_animation_frame_count / loading_animation_frame_count;

    initialized = true;
  }
}

