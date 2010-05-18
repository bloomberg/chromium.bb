// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/content_settings_types.h"
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
                                public InfoBubbleDelegate {
 public:
  ContentSettingImageView(ContentSettingsType content_type,
                          const LocationBarView* parent,
                          Profile* profile);
  virtual ~ContentSettingImageView();

  void set_profile(Profile* profile) { profile_ = profile; }
  void UpdateFromTabContents(const TabContents* tab_contents);

 private:
  // views::ImageView overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  // InfoBubbleDelegate overrides:
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow() { return false; }

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  // The owning LocationBarView.
  const LocationBarView* parent_;

  // The currently active profile.
  Profile* profile_;

  // The currently shown info bubble if any.
  InfoBubble* info_bubble_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
