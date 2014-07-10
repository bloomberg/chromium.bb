// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"


namespace {
const int kBackgroundImages[] = IMAGE_GRID(IDR_OMNIBOX_CONTENT_SETTING_BUBBLE);
const int kStayOpenTimeMS = 3200;  // Time spent with animation fully open.
}


// static
const int ContentSettingImageView::kOpenTimeMS = 150;
const int ContentSettingImageView::kAnimationDurationMS =
    (kOpenTimeMS * 2) + kStayOpenTimeMS;

ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    LocationBarView* parent,
    const gfx::FontList& font_list,
    SkColor text_color,
    SkColor parent_background_color)
    : parent_(parent),
      content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      background_painter_(
          views::Painter::CreateImageGridPainter(kBackgroundImages)),
      icon_(new views::ImageView),
      text_label_(new views::Label(base::string16(), font_list)),
      slide_animator_(this),
      pause_animation_(false),
      pause_animation_state_(0.0),
      bubble_widget_(NULL) {
  icon_->SetHorizontalAlignment(views::ImageView::LEADING);
  AddChildView(icon_);

  text_label_->SetVisible(false);
  text_label_->SetEnabledColor(text_color);
  // Calculate the actual background color for the label.  The background images
  // are painted atop |parent_background_color|.  We grab the color of the
  // middle pixel of the middle image of the background, which we treat as the
  // representative color of the entire background (reasonable, given the
  // current appearance of these images).  Then we alpha-blend it over the
  // parent background color to determine the actual color the label text will
  // sit atop.
  const SkBitmap& bitmap(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          kBackgroundImages[4])->GetRepresentation(1.0f).sk_bitmap());
  SkAutoLockPixels pixel_lock(bitmap);
  SkColor background_image_color =
      bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  // Tricky bit: We alpha blend an opaque version of |background_image_color|
  // against |parent_background_color| using the original image grid color's
  // alpha. This is because AlphaBlend(a, b, 255) always returns |a| unchanged
  // even if |a| is a color with non-255 alpha.
  text_label_->SetBackgroundColor(
      color_utils::AlphaBlend(SkColorSetA(background_image_color, 255),
                              parent_background_color,
                              SkColorGetA(background_image_color)));
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetElideBehavior(gfx::NO_ELIDE);
  AddChildView(text_label_);

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

  icon_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      content_setting_image_model_->get_icon()));
  icon_->SetTooltipText(
      base::UTF8ToUTF16(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  // If the content blockage should be indicated to the user, start the
  // animation and record that we indicated the blockage.
  TabSpecificContentSettings* content_settings = web_contents ?
      TabSpecificContentSettings::FromWebContents(web_contents) : NULL;
  if (!content_settings || content_settings->IsBlockageIndicated(
      content_setting_image_model_->get_content_settings_type()))
    return;

  // We just ignore this blockage if we're already showing some other string to
  // the user.  If this becomes a problem, we could design some sort of queueing
  // mechanism to show one after the other, but it doesn't seem important now.
  int string_id = content_setting_image_model_->explanatory_string_id();
  if (string_id && !background_showing()) {
    text_label_->SetText(l10n_util::GetStringUTF16(string_id));
    text_label_->SetVisible(true);
    slide_animator_.Show();
  }

  content_settings->SetBlockageHasBeenIndicated(
      content_setting_image_model_->get_content_settings_type());
}

// static
int ContentSettingImageView::GetBubbleOuterPadding(bool by_icon) {
  return LocationBarView::kItemPadding - LocationBarView::kBubblePadding +
      (by_icon ? 0 : LocationBarView::kIconInternalPadding);
}

void ContentSettingImageView::AnimationEnded(const gfx::Animation* animation) {
  slide_animator_.Reset();
  if (!pause_animation_) {
    text_label_->SetVisible(false);
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

gfx::Size ContentSettingImageView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  gfx::Size size(icon_->GetPreferredSize());
  if (background_showing()) {
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
    size.Enlarge(
        size_fraction * (text_label_->GetPreferredSize().width() +
            GetTotalSpacingWhileAnimating()), 0);
    size.SetToMax(background_painter_->GetMinimumSize());
  }
  return size;
}

void ContentSettingImageView::Layout() {
  const int icon_width = icon_->GetPreferredSize().width();
  icon_->SetBounds(
      std::min((width() - icon_width) / 2, GetBubbleOuterPadding(true)), 0,
      icon_width, height());
  text_label_->SetBounds(
      icon_->bounds().right() + LocationBarView::kItemPadding, 0,
      std::max(width() - GetTotalSpacingWhileAnimating() - icon_width, 0),
      height());
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

void ContentSettingImageView::OnPaintBackground(gfx::Canvas* canvas) {
  if (background_showing())
    background_painter_->Paint(canvas, size());
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

int ContentSettingImageView::GetTotalSpacingWhileAnimating() const {
  return GetBubbleOuterPadding(true) + LocationBarView::kItemPadding +
      GetBubbleOuterPadding(false);
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
            ContentSettingBubbleModel::CreateContentSettingBubbleModel(
                parent_->delegate()->GetContentSettingBubbleModelDelegate(),
                web_contents, parent_->profile(),
                content_setting_image_model_->get_content_settings_type()),
            web_contents, this, views::BubbleBorder::TOP_RIGHT));
    bubble_widget_->AddObserver(this);
    bubble_widget_->Show();
  }
}
