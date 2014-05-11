// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/location_bar/image_decoration.h"
#include "chrome/common/content_settings_types.h"

// ContentSettingDecoration is used to display the content settings
// images on the current page.

@class ContentSettingAnimationState;
class ContentSettingImageModel;
class LocationBarViewMac;
class Profile;

namespace content {
class WebContents;
}

class ContentSettingDecoration : public ImageDecoration {
 public:
  ContentSettingDecoration(ContentSettingsType settings_type,
                           LocationBarViewMac* owner,
                           Profile* profile);
  virtual ~ContentSettingDecoration();

  // Updates the image and visibility state based on the supplied WebContents.
  // Returns true if the decoration's visible state changed.
  bool UpdateFromWebContents(content::WebContents* web_contents);

  // Overridden from |LocationBarDecoration|
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

  // Called from internal animator. Only public because ObjC objects can't
  // be friends.
  virtual void AnimationTimerFired();

 private:

  void SetToolTip(NSString* tooltip);

  // Returns an attributed string with the animated text.
  base::scoped_nsobject<NSAttributedString> CreateAnimatedText();

  // Measure the width of the animated text.
  CGFloat MeasureTextWidth();

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  LocationBarViewMac* owner_;  // weak
  Profile* profile_;  // weak

  base::scoped_nsobject<NSString> tooltip_;

  // Used when the decoration has animated text.
  base::scoped_nsobject<ContentSettingAnimationState> animation_;
  CGFloat text_width_;
  base::scoped_nsobject<NSAttributedString> animated_text_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CONTENT_SETTING_DECORATION_H_
