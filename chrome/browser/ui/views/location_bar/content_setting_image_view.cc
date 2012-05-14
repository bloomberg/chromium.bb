// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/tween.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"

using content::WebContents;

namespace {
// Animation parameters.
const int kOpenTimeMs = 150;
const int kFullOpenedTimeMs = 3200;
const int kMoveTimeMs = kFullOpenedTimeMs + 2 * kOpenTimeMs;
const int kFrameRateHz = 60;
// Colors for the animated box.
const SkColor kTopBoxColor = SkColorSetRGB(0xff, 0xf8, 0xd4);
const SkColor kBottomBoxColor = SkColorSetRGB(0xff, 0xe6, 0xaf);
const SkColor kBorderColor = SkColorSetRGB(0xe9, 0xb9, 0x66);
// Corner radius of the animated box.
const SkScalar kBoxCornerRadius = 2;
// Margins for animated box.
const int kTextMarginPixels = 4;
const int kIconLeftMargin = 4;


// The fraction of the animation we'll spend animating the string into view, and
// then again animating it closed -  total animation (slide out, show, then
// slide in) is 1.0.
const double kAnimatingFraction = kOpenTimeMs * 1.0 / kMoveTimeMs;
}

ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    LocationBarView* parent)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      bubble_widget_(NULL),
      parent_(parent),
      pause_animation_(false),
      text_size_(0),
      visible_text_size_(0) {
  SetHorizontalAlignment(ImageView::LEADING);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }
}

void ContentSettingImageView::UpdateFromWebContents(WebContents* web_contents) {
  content_setting_image_model_->UpdateFromWebContents(web_contents);
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  SetImage(ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
      content_setting_image_model_->get_icon()));
  SetTooltipText(UTF8ToUTF16(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  TabSpecificContentSettings* content_settings = NULL;
  if (web_contents) {
    content_settings = TabContentsWrapper::GetCurrentWrapperForContents(
        web_contents)->content_settings();
  }
  if (!content_settings || content_settings->IsBlockageIndicated(
      content_setting_image_model_->get_content_settings_type()))
    return;

  // The content blockage was not yet indicated to the user. Start indication
  // animation and clear "not yet shown" flag.
  content_settings->SetBlockageHasBeenIndicated(
      content_setting_image_model_->get_content_settings_type());

  int animated_string_id =
      content_setting_image_model_->explanatory_string_id();
  // Check if the string for animation is available.
  if (!animated_string_id)
    return;

  if (!slide_animator_.get()) {
    slide_animator_.reset(new ui::SlideAnimation(this));
    slide_animator_->SetSlideDuration(kMoveTimeMs);
    slide_animator_->SetTweenType(ui::Tween::LINEAR);
  }
  // Do not start animation if already in progress.
  if (!slide_animator_->is_animating()) {
    // Initialize animated string. It will be cleared when animation is
    // completed.
    animated_text_ = l10n_util::GetStringUTF16(animated_string_id);
    text_size_ = ui::ResourceBundle::GetSharedInstance().GetFont(
        ui::ResourceBundle::MediumFont).GetStringWidth(animated_text_);
    text_size_ += 2 * kTextMarginPixels + kIconLeftMargin;
    if (border())
      border()->GetInsets(&saved_insets_);
    slide_animator_->Show();
  }
}

gfx::Size ContentSettingImageView::GetPreferredSize() {
  gfx::Size preferred_size(views::ImageView::GetPreferredSize());
  // When view is animated visible_text_size_ > 0, it is 0 otherwise.
  preferred_size.set_width(preferred_size.width() + visible_text_size_);
  return preferred_size;
}

void ContentSettingImageView::AnimationEnded(const ui::Animation* animation) {
  if (pause_animation_)
    pause_animation_ = false;
  slide_animator_->Reset();
}

void ContentSettingImageView::AnimationProgressed(
    const ui::Animation* animation) {
  if (pause_animation_)
    return;
  double state = slide_animator_->GetCurrentValue();
  if (state >= 1.0) {
    // Animaton is over, clear the variables.
    visible_text_size_ = 0;
  } else if (state < kAnimatingFraction) {
    visible_text_size_ = static_cast<int>(text_size_ * state /
                                          kAnimatingFraction);
  } else if (state > (1.0 - kAnimatingFraction)) {
    visible_text_size_ = static_cast<int>(text_size_ * (1.0 - state) /
                                          kAnimatingFraction);
  } else {
    visible_text_size_ = text_size_;
  }
  parent_->Layout();
  parent_->SchedulePaint();
}

void ContentSettingImageView::AnimationCanceled(
    const ui::Animation* animation) {
  AnimationEnded(animation);
}

bool ContentSettingImageView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ContentSettingImageView::OnMouseReleased(const views::MouseEvent& event) {
  if (!HitTest(event.location()))
    return;

  TabContentsWrapper* tab_contents = parent_->GetTabContentsWrapper();
  if (!tab_contents)
    return;

  // Stop animation.
  if (slide_animator_.get() && slide_animator_->is_animating()) {
    slide_animator_->Reset();
    pause_animation_ = true;
  }

  Profile* profile = parent_->profile();
  ContentSettingBubbleContents* bubble = new ContentSettingBubbleContents(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          parent_->delegate()->GetContentSettingBubbleModelDelegate(),
          tab_contents,
          profile,
          content_setting_image_model_->get_content_settings_type()),
      profile,
      tab_contents->web_contents(),
      this,
      views::BubbleBorder::TOP_RIGHT);
  bubble_widget_ = parent_->delegate()->CreateViewsBubble(bubble);
  bubble_widget_->AddObserver(this);
  bubble->Show();
}

void ContentSettingImageView::OnPaint(gfx::Canvas* canvas) {
  // During the animation we draw a border, an icon and the text. The text area
  // is changing in size during the animation, giving the appearance of the text
  // sliding out and then back in. When the text completely slid out the yellow
  // border is no longer painted around the icon. |visible_text_size_| is 0 when
  // animation is stopped.
  if (slide_animator_.get() &&
      (slide_animator_->is_animating() || pause_animation_)) {
    // In the non-animated state borders' left() is 0, in the animated state it
    // is the kIconLeftMargin, so we need to animate border reduction when it
    // starts to disappear.
    int necessary_left_margin = std::min(kIconLeftMargin, visible_text_size_);
    views::Border* empty_border = views::Border::CreateEmptyBorder(
        saved_insets_.top(),
        saved_insets_.left() + necessary_left_margin,
        saved_insets_.bottom(),
        saved_insets_.right());
    set_border(empty_border);
    views::ImageView::OnPaint(canvas);

    // Paint text to the right of the icon.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    canvas->DrawStringInt(animated_text_,
        rb.GetFont(ui::ResourceBundle::MediumFont), SK_ColorBLACK,
        GetImageBounds().right() + kTextMarginPixels, y(),
        width() - GetImageBounds().width(), height(),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::TEXT_VALIGN_MIDDLE);
  } else {
    views::ImageView::OnPaint(canvas);
  }
}

void ContentSettingImageView::OnPaintBackground(gfx::Canvas* canvas) {
  if (slide_animator_.get() &&
      (slide_animator_->is_animating() || pause_animation_)) {
    // Paint yellow gradient background if in animation mode.
    const int kEdgeThickness = 1;
    SkPaint paint;
    paint.setShader(gfx::CreateGradientShader(kEdgeThickness,
                    height() - (2 * kEdgeThickness),
                    kTopBoxColor, kBottomBoxColor));
    SkSafeUnref(paint.getShader());
    SkRect color_rect;
    color_rect.iset(0, 0, width() - 1, height() - 1);
    canvas->sk_canvas()->drawRoundRect(color_rect, kBoxCornerRadius,
                                       kBoxCornerRadius, paint);
    SkPaint outer_paint;
    outer_paint.setStyle(SkPaint::kStroke_Style);
    outer_paint.setColor(kBorderColor);
    color_rect.inset(SkIntToScalar(kEdgeThickness),
                     SkIntToScalar(kEdgeThickness));
    canvas->sk_canvas()->drawRoundRect(color_rect, kBoxCornerRadius,
                                       kBoxCornerRadius, outer_paint);
  } else {
    views::ImageView::OnPaintBackground(canvas);
    return;
  }
}

void ContentSettingImageView::OnWidgetClosing(views::Widget* widget) {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }
  if (pause_animation_) {
    slide_animator_->Reset(
        1.0 - (visible_text_size_ * kAnimatingFraction) / text_size_);
    pause_animation_ = false;
    slide_animator_->Show();
  }
}
