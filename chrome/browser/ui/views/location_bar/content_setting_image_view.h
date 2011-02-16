// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/views/info_bubble.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/animation/linear_animation.h"
#include "views/controls/image_view.h"

class ContentSettingImageModel;
class InfoBubble;
class LocationBarView;
class Profile;
class TabContents;

namespace views {
class MouseEvent;
}

class ContentSettingImageView : public views::ImageView,
                                public InfoBubbleDelegate,
                                public ui::LinearAnimation {
 public:
  ContentSettingImageView(ContentSettingsType content_type,
                          LocationBarView* parent,
                          Profile* profile);
  virtual ~ContentSettingImageView();

  void set_profile(Profile* profile) { profile_ = profile; }
  // |new_navigation| true if this is a new navigation, false if the tab was
  // just switched to.
  void UpdateFromTabContents(TabContents* tab_contents);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize();

 private:
  // views::ImageView overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual void VisibilityChanged(View* starting_from, bool is_visible);
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void OnPaintBackground(gfx::Canvas* canvas);

  // InfoBubbleDelegate overrides:
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow() { return false; }

  // ui::LinearAnimation override:
  virtual void AnimateToState(double state);

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  // The owning LocationBarView.
  LocationBarView* parent_;

  // The currently active profile.
  Profile* profile_;

  // The currently shown info bubble if any.
  InfoBubble* info_bubble_;

  string16 animated_text_;
  bool animation_in_progress_;
  int text_size_;
  int visible_text_size_;
  gfx::Insets saved_insets_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
