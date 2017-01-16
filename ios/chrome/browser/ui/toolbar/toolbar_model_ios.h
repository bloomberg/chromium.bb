// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IOS_H_

#import <UIKit/UIKit.h>

#include "components/toolbar/toolbar_model.h"

// This class adds some IOS specific methods to the ToolbarModel.
// It does not inherit from ToolbarModel on purpose.
class ToolbarModelIOS {
 public:
  virtual ~ToolbarModelIOS() {}

  // Returns the |ToolbarModel| contained by this instance.
  virtual ToolbarModel* GetToolbarModel() = 0;

  // Returns true if the current tab is currently loading.
  virtual bool IsLoading() = 0;

  // Returns the fraction of the current tab's page load that has completed as a
  // number between 0.0 and 1.0.
  virtual CGFloat GetLoadProgressFraction() = 0;

  // Returns true if the current tab can go back in history.
  virtual bool CanGoBack() = 0;

  // Returns true if the current tab can go forward in history.
  virtual bool CanGoForward() = 0;

  // Returns true if the current tab is a native page.
  virtual bool IsCurrentTabNativePage() = 0;

  // Returns true if the current tab's URL is bookmarked, either by the user or
  // by a managed bookmarks.
  virtual bool IsCurrentTabBookmarked() = 0;

  // Returns true if the current tab's URL is bookmarked by the user; returns
  // false if the URL is bookmarked only in managed bookmarks.
  virtual bool IsCurrentTabBookmarkedByUser() = 0;

  // Returns true if the current toolbar should display the hint text.
  virtual bool ShouldDisplayHintText() = 0;

 protected:
  ToolbarModelIOS() {}
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IOS_H_
