// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/animation/linear_animation.h"
#include "views/controls/image_view.h"

class ContentSettingImageModel;
class ContentSettingBubbleContents;
class LocationBarView;
class TabContents;

namespace views {
class MouseEvent;
}

class ContentSettingsDelegateView;

class ContentSettingImageView : public views::ImageView,
                                public ui::LinearAnimation {
 public:
  ContentSettingImageView(ContentSettingsType content_type,
                          LocationBarView* parent);
  virtual ~ContentSettingImageView();

  // |new_navigation| true if this is a new navigation, false if the tab was
  // just switched to.
  void UpdateFromTabContents(TabContents* tab_contents);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize();

 private:
  // views::ImageView overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // ui::LinearAnimation override:
  virtual void AnimateToState(double state) OVERRIDE;

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  // The owning LocationBarView.
  LocationBarView* parent_;

  string16 animated_text_;
  bool animation_in_progress_;
  int text_size_;
  int visible_text_size_;
  gfx::Insets saved_insets_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
