// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar/content_setting_image_view.h"

#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/content_setting_bubble_contents.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"

ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    const LocationBarView* parent,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      parent_(parent),
      profile_(profile),
      info_bubble_(NULL) {
}

ContentSettingImageView::~ContentSettingImageView() {
  if (info_bubble_)
    info_bubble_->Close();
}

void ContentSettingImageView::UpdateFromTabContents(
    const TabContents* tab_contents) {
  int old_icon = content_setting_image_model_->get_icon();
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  if (old_icon != content_setting_image_model_->get_icon()) {
    SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
        content_setting_image_model_->get_icon()));
  }
  SetTooltipText(UTF8ToWide(content_setting_image_model_->get_tooltip()));
  SetVisible(true);
}

bool ContentSettingImageView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ContentSettingImageView::OnMouseReleased(const views::MouseEvent& event,
                                              bool canceled) {
  if (canceled || !HitTest(event.location()))
    return;

  TabContents* tab_contents = parent_->GetTabContents();
  if (!tab_contents)
    return;

  gfx::Rect screen_bounds(GetImageBounds());
  gfx::Point origin(screen_bounds.origin());
  views::View::ConvertPointToScreen(this, &origin);
  screen_bounds.set_origin(origin);
  ContentSettingBubbleContents* bubble_contents =
      new ContentSettingBubbleContents(
          ContentSettingBubbleModel::CreateContentSettingBubbleModel(
              tab_contents, profile_,
              content_setting_image_model_->get_content_settings_type()),
          profile_, tab_contents);
  info_bubble_ = InfoBubble::Show(GetWidget(), screen_bounds,
      BubbleBorder::TOP_RIGHT, bubble_contents, this);
  bubble_contents->set_info_bubble(info_bubble_);
}

void ContentSettingImageView::VisibilityChanged(View* starting_from,
                                                bool is_visible) {
  if (!is_visible && info_bubble_)
    info_bubble_->Close();
}

void ContentSettingImageView::InfoBubbleClosing(InfoBubble* info_bubble,
                                                bool closed_by_escape) {
  info_bubble_ = NULL;
}

bool ContentSettingImageView::CloseOnEscape() {
  return true;
}

