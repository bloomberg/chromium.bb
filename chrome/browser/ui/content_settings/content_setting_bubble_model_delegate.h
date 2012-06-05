// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#pragma once

#include "chrome/common/content_settings_types.h"

class TabContents;
typedef TabContents TabContentsWrapper;

// Delegate which is used by ContentSettingBubbleModel class.
class ContentSettingBubbleModelDelegate {
 public:
  // Shows the cookies collected in the tab contents wrapper.
  virtual void ShowCollectedCookiesDialog(TabContentsWrapper* contents) = 0;

  // Shows the Content Settings page for a given content type.
  virtual void ShowContentSettingsPage(ContentSettingsType type) = 0;

 protected:
  virtual ~ContentSettingBubbleModelDelegate() {}
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
