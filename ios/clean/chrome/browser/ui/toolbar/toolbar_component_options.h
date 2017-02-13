// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COMPONENT_OPTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COMPONENT_OPTIONS_H_

// List of different SizeClass states. If a Visibility Mask bit is TRUE then the
// component could be visible for that SizeClass. In order for a component to be
// visible it needs to meet all current conditions.
typedef NS_OPTIONS(NSUInteger, ToolbarComponentVisibility) {
  ToolbarComponentVisibilityNone = 0,
  ToolbarComponentVisibilityCompactWidth = 1 << 0,
  ToolbarComponentVisibilityRegularWidth = 1 << 1,
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COMPONENT_OPTIONS_H_
