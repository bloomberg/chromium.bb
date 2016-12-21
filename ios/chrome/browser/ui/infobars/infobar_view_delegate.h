// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

// Interface for delegating events from infobar.
class InfoBarViewDelegate {
 public:
  // Notifies that the target size has been changed (e.g. after rotation).
  virtual void SetInfoBarTargetHeight(int height) = 0;

  // Notifies that the close button was pressed.
  virtual void InfoBarDidCancel() = 0;

  // Notifies that an infobar button was pressed.
  virtual void InfoBarButtonDidPress(NSUInteger button_id) = 0;

 protected:
  virtual ~InfoBarViewDelegate() {}
};

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_DELEGATE_H_
