// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"

class Browser;

// Implementation of ContentSettingBubbleModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserContentSettingBubbleModelDelegate
    : public ContentSettingBubbleModelDelegate {
 public:
  explicit BrowserContentSettingBubbleModelDelegate(Browser* browser);
  virtual ~BrowserContentSettingBubbleModelDelegate();

  // ContentSettingBubbleModelDelegate implementation:
  virtual void ShowCollectedCookiesDialog(
      content::WebContents* web_contents) OVERRIDE;
  virtual void ShowContentSettingsPage(ContentSettingsType type) OVERRIDE;
  virtual void ShowLearnMorePage(ContentSettingsType type) OVERRIDE;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContentSettingBubbleModelDelegate);
};

#endif  // CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
