// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {
// Animation parameters.
const int kOpenTimeMs = 150;
const int kFullOpenedTimeMs = 3200;
const int kMoveTimeMs = kFullOpenedTimeMs + 2 * kOpenTimeMs;


// The fraction of the animation we'll spend animating the string into view, and
// then again animating it closed -  total animation (slide out, show, then
// slide in) is 1.0.
const double kAnimatingFraction = kOpenTimeMs * 1.0 / kMoveTimeMs;
}

ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    const int background_images[],
    LocationBarView* parent)
    : LocationBarDecorationView(parent, background_images),
      content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      bubble_widget_(NULL) {
  SetHorizontalAlignment(ImageView::LEADING);
  TouchableLocationBarView::Init(this);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }
}

void ContentSettingImageView::Update(TabContents* tab_contents) {
  if (tab_contents) {
    content_setting_image_model_->UpdateFromWebContents(
        tab_contents->web_contents());
  }
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      content_setting_image_model_->get_icon()));
  SetTooltipText(UTF8ToUTF16(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  TabSpecificContentSettings* content_settings = NULL;
  if (tab_contents)
    content_settings = tab_contents->content_settings();

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

  StartLabelAnimation(l10n_util::GetStringUTF16(animated_string_id),
                      kMoveTimeMs);
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

void ContentSettingImageView::OnWidgetClosing(views::Widget* widget) {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_ = NULL;
  }

  UnpauseAnimation();
}

void ContentSettingImageView::OnClick(LocationBarView* parent) {
  TabContents* tab_contents = parent->GetTabContents();
  if (!tab_contents)
    return;
  if (bubble_widget_)
    return;

  Profile* profile = parent->profile();
  ContentSettingBubbleContents* bubble = new ContentSettingBubbleContents(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          parent->delegate()->GetContentSettingBubbleModelDelegate(),
          tab_contents,
          profile,
          content_setting_image_model_->get_content_settings_type()),
      tab_contents->web_contents(),
      this,
      views::BubbleBorder::TOP_RIGHT);
  bubble_widget_ = parent->delegate()->CreateViewsBubble(bubble);
  bubble_widget_->AddObserver(this);
  bubble->Show();
}
