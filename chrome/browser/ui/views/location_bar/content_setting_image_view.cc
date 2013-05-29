// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
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
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using content::WebContents;


namespace {
// Animation parameters.
const int kFrameRateHz = 60;
const int kOpenTimeMs = 150;
const int kFullOpenedTimeMs = 3200;
const int kMoveTimeMs = kFullOpenedTimeMs + 2 * kOpenTimeMs;

// The fraction of the animation we'll spend animating the string into view, and
// then again animating it closed -  total animation (slide out, show, then
// slide in) is 1.0.
const double kAnimatingFraction = kOpenTimeMs * 1.0 / kMoveTimeMs;

// Margins for animated box (pixels).
const int kHorizMargin = 4;
const int kIconLabelSpacing = 4;
}


ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    const int background_images[],
    LocationBarView* parent,
    const gfx::Font& font,
    int font_y_offset,
    SkColor font_color)
    : parent_(parent),
      content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      background_painter_(background_images),
      icon_(new views::ImageView),
      text_label_(NULL),
      slide_animator_(this),
      pause_animation_(false),
      pause_animation_state_(0.0),
      bubble_widget_(NULL),
      font_(font),
      text_size_(0),
      visible_text_size_(0) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  icon_->SetHorizontalAlignment(views::ImageView::LEADING);
  AddChildView(icon_);
  TouchableLocationBarView::Init(this);

  slide_animator_.SetSlideDuration(kMoveTimeMs);
  slide_animator_.SetTweenType(ui::Tween::LINEAR);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }
}

int ContentSettingImageView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}

void ContentSettingImageView::Update(WebContents* web_contents) {
  if (web_contents) {
    content_setting_image_model_->UpdateFromWebContents(web_contents);
  }
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  icon_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      content_setting_image_model_->get_icon()));
  icon_->SetTooltipText(
      UTF8ToUTF16(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  TabSpecificContentSettings* content_settings = NULL;
  if (web_contents) {
    content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents);
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

  // Do not start animation if already in progress.
  if (!slide_animator_.is_animating()) {
    // Initialize animated string. It will be cleared when animation is
    // completed.
    if (!text_label_) {
      text_label_ = new views::Label;
      text_label_->SetElideBehavior(views::Label::NO_ELIDE);
      text_label_->SetFont(font_);
      SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kHorizontal, kHorizMargin, 0, kIconLabelSpacing));
      AddChildView(text_label_);
    }
    text_label_->SetText(l10n_util::GetStringUTF16(animated_string_id));
    text_size_ = font_.GetStringWidth(text_label_->text());
    text_size_ += kHorizMargin;
    slide_animator_.Show();
  }
}

void ContentSettingImageView::AnimationEnded(const ui::Animation* animation) {
  if (!pause_animation_) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    RemoveChildView(text_label_);  // will also delete the view.
    text_label_ = NULL;
    parent_->Layout();
    parent_->SchedulePaint();
  }
  slide_animator_.Reset();
}

void ContentSettingImageView::AnimationProgressed(
    const ui::Animation* animation) {
  if (pause_animation_)
    return;

  visible_text_size_ = GetTextAnimationSize(slide_animator_.GetCurrentValue(),
                                            text_size_);

  parent_->Layout();
  parent_->SchedulePaint();
}

void ContentSettingImageView::AnimationCanceled(
    const ui::Animation* animation) {
  AnimationEnded(animation);
}

gfx::Size ContentSettingImageView::GetPreferredSize() {
  // Height will be ignored by the LocationBarView.
  gfx::Size preferred_size(views::View::GetPreferredSize());
  int non_label_width = preferred_size.width() -
      (text_label_ ? text_label_->GetPreferredSize().width() : 0);
  // When view is animated |visible_text_size_| > 0, it is 0 otherwise.
  preferred_size.set_width(non_label_width + visible_text_size_);
  return preferred_size;
}

bool ContentSettingImageView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ContentSettingImageView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    return;

  OnClick(parent_);
}

void ContentSettingImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    OnClick(parent_);
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    event->SetHandled();
  }
}

void ContentSettingImageView::OnPaintBackground(gfx::Canvas* canvas) {
  if (slide_animator_.is_animating() || pause_animation_)
    background_painter_.Paint(canvas, size());
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }

  if (!pause_animation_)
    return;

  slide_animator_.Reset(pause_animation_state_);
  pause_animation_ = false;
  slide_animator_.Show();
}

void ContentSettingImageView::OnClick(LocationBarView* parent) {
  // Stop animation.
  if (slide_animator_.is_animating()) {
    if (!pause_animation_) {
      pause_animation_ = true;
      pause_animation_state_ = slide_animator_.GetCurrentValue();
    }
    slide_animator_.Reset();
  }

  WebContents* web_contents = parent->GetWebContents();
  if (!web_contents)
    return;
  if (bubble_widget_)
    return;

  Profile* profile = parent->profile();
  ContentSettingBubbleContents* bubble = new ContentSettingBubbleContents(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          parent->delegate()->GetContentSettingBubbleModelDelegate(),
          web_contents,
          profile,
          content_setting_image_model_->get_content_settings_type()),
      web_contents,
      this,
      views::BubbleBorder::TOP_RIGHT);
  bubble_widget_ = parent->delegate()->CreateViewsBubble(bubble);
  bubble_widget_->AddObserver(this);
  bubble->GetWidget()->Show();
}

int ContentSettingImageView::GetTextAnimationSize(double state,
                                                  int text_size) {
  if (state >= 1.0) {
    // Animaton is over, clear the variables.
    return 0;
  } else if (state < kAnimatingFraction) {
    return static_cast<int>(text_size * state / kAnimatingFraction);
  } else if (state > (1.0 - kAnimatingFraction)) {
    return static_cast<int>(text_size * (1.0 - state) / kAnimatingFraction);
  } else {
    return text_size;
  }
}
