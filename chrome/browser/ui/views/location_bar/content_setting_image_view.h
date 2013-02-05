// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"
#include "chrome/common/content_settings_types.h"
#include "ui/views/widget/widget_observer.h"

class ContentSettingImageModel;
class LocationBarView;

// The ContentSettingImageView displays an icon and optional text label for
// various content settings affordances in the location bar (i.e. plugin
// blocking, geolocation).
class ContentSettingImageView : public LocationBarDecorationView,
                                public views::WidgetObserver {
 public:
  ContentSettingImageView(ContentSettingsType content_type,
                          const int background_images[],
                          LocationBarView* parent,
                          const gfx::Font& font,
                          SkColor font_color);
  virtual ~ContentSettingImageView();

  virtual void Update(content::WebContents* web_contents) OVERRIDE;


  // views::WidgetObserver override:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 protected:
  // Invoked when the user clicks on the control.
  virtual void OnClick(LocationBarView* parent) OVERRIDE;
  virtual int GetTextAnimationSize(double state, int text_size) OVERRIDE;

 private:
  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  views::Widget* bubble_widget_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
