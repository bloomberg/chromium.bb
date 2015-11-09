// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"


namespace {
// Time spent with animation fully open.
const int kStayOpenTimeMS = 3200;
}


// static
const int ContentSettingImageView::kOpenTimeMS = 150;
const int ContentSettingImageView::kAnimationDurationMS =
    (kOpenTimeMS * 2) + kStayOpenTimeMS;

ContentSettingImageView::ContentSettingImageView(
    ContentSettingImageModel* image_model,
    LocationBarView* parent,
    const gfx::FontList& font_list,
    SkColor text_color,
    SkColor parent_background_color)
    : IconLabelBubbleView(0,
                          font_list,
                          text_color,
                          parent_background_color,
                          false),
      parent_(parent),
      content_setting_image_model_(image_model),
      slide_animator_(this),
      pause_animation_(false),
      pause_animation_state_(0.0),
      bubble_widget_(NULL) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    static const int kBackgroundImages[] =
        IMAGE_GRID(IDR_OMNIBOX_CONTENT_SETTING_BUBBLE);
    SetBackgroundImageGrid(kBackgroundImages);
  }

  image()->SetHorizontalAlignment(views::ImageView::LEADING);
  image()->set_interactive(true);
  label()->SetElideBehavior(gfx::NO_ELIDE);

  slide_animator_.SetSlideDuration(kAnimationDurationMS);
  slide_animator_.SetTweenType(gfx::Tween::LINEAR);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (bubble_widget_)
    bubble_widget_->RemoveObserver(this);
}

void ContentSettingImageView::Update(content::WebContents* web_contents) {
  // Note: We explicitly want to call this even if |web_contents| is NULL, so we
  // get hidden properly while the user is editing the omnibox.
  content_setting_image_model_->UpdateFromWebContents(web_contents);

  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }

  SetImage(content_setting_image_model_->icon().AsImageSkia());
  image()->SetTooltipText(
      base::UTF8ToUTF16(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  // If the content usage or blockage should be indicated to the user, start the
  // animation and record that the icon has been shown.
  if (!content_setting_image_model_->ShouldRunAnimation(web_contents))
    return;

  // We just ignore this blockage if we're already showing some other string to
  // the user.  If this becomes a problem, we could design some sort of queueing
  // mechanism to show one after the other, but it doesn't seem important now.
  int string_id = content_setting_image_model_->explanatory_string_id();
  if (string_id && !ShouldShowBackground()) {
    SetLabel(l10n_util::GetStringUTF16(string_id));
    label()->SetVisible(true);
    slide_animator_.Show();
  }

  content_setting_image_model_->SetAnimationHasRun(web_contents);
}

SkColor ContentSettingImageView::GetTextColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

SkColor ContentSettingImageView::GetBorderColor() const {
  return gfx::kGoogleYellow700;
}

bool ContentSettingImageView::ShouldShowBackground() const {
  return slide_animator_.is_animating() || pause_animation_;
}

double ContentSettingImageView::WidthMultiplier() const {
  double state = slide_animator_.GetCurrentValue();
  // The fraction of the animation we'll spend animating the string into view,
  // which is also the fraction we'll spend animating it closed; total
  // animation (slide out, show, then slide in) is 1.0.
  const double kOpenFraction =
      static_cast<double>(kOpenTimeMS) / kAnimationDurationMS;
  double size_fraction = 1.0;
  if (state < kOpenFraction)
    size_fraction = state / kOpenFraction;
  if (state > (1.0 - kOpenFraction))
    size_fraction = (1.0 - state) / kOpenFraction;
  return size_fraction;
}

void ContentSettingImageView::AnimationEnded(const gfx::Animation* animation) {
  slide_animator_.Reset();
  if (!pause_animation_) {
    label()->SetVisible(false);
    parent_->Layout();
    parent_->SchedulePaint();
  }
}

void ContentSettingImageView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (!pause_animation_) {
    parent_->Layout();
    parent_->SchedulePaint();
  }
}

void ContentSettingImageView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

const char* ContentSettingImageView::GetClassName() const {
  return "ContentSettingsImageView";
}

bool ContentSettingImageView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ContentSettingImageView::OnMouseReleased(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location()))
    OnClick();
}

void ContentSettingImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP)
    OnClick();
  if ((event->type() == ui::ET_GESTURE_TAP) ||
      (event->type() == ui::ET_GESTURE_TAP_DOWN))
    event->SetHandled();
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = NULL;

  if (pause_animation_) {
    slide_animator_.Reset(pause_animation_state_);
    pause_animation_ = false;
    slide_animator_.Show();
  }
}

void ContentSettingImageView::OnClick() {
  if (slide_animator_.is_animating()) {
    if (!pause_animation_) {
      pause_animation_ = true;
      pause_animation_state_ = slide_animator_.GetCurrentValue();
    }
    slide_animator_.Reset();
  }

  content::WebContents* web_contents = parent_->GetWebContents();
  if (web_contents && !bubble_widget_) {
    bubble_widget_ =
        parent_->delegate()->CreateViewsBubble(new ContentSettingBubbleContents(
            content_setting_image_model_->CreateBubbleModel(
                parent_->delegate()->GetContentSettingBubbleModelDelegate(),
                web_contents, parent_->profile()),
            web_contents, this, views::BubbleBorder::TOP_RIGHT));
    bubble_widget_->AddObserver(this);
    bubble_widget_->Show();
  }
}
