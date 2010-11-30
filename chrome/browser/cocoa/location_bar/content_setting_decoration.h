// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_
#pragma once

#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/location_bar/image_decoration.h"
#include "chrome/common/content_settings_types.h"

// ContentSettingDecoration is used to display the content settings
// images on the current page.

class ContentSettingImageModel;
class LocationBarViewMac;
class Profile;
class TabContents;

class ContentSettingDecoration : public ImageDecoration {
 public:
  ContentSettingDecoration(ContentSettingsType settings_type,
                           LocationBarViewMac* owner,
                           Profile* profile);
  virtual ~ContentSettingDecoration();

  // Updates the image and visibility state based on the supplied TabContents.
  // Returns true if the decoration's visible state changed.
  bool UpdateFromTabContents(TabContents* tab_contents);

  // Overridden from |LocationBarDecoration|
  virtual bool AcceptsMousePress() { return true; }
  virtual bool OnMousePressed(NSRect frame);
  virtual NSString* GetToolTip();

 private:
  // Helper to get where the bubble point should land.  Similar to
  // |PageActionDecoration| or |StarDecoration| (|LocationBarViewMac|
  // calls those).
  NSPoint GetBubblePointInFrame(NSRect frame);

  void SetToolTip(NSString* tooltip);

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  LocationBarViewMac* owner_;  // weak
  Profile* profile_;  // weak

  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingDecoration);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_
