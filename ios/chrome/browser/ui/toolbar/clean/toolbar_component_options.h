// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COMPONENT_OPTIONS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COMPONENT_OPTIONS_H_

// List of different SizeClass states. If a Visibility Mask bit is TRUE then the
// component could be visible for that SizeClass. In order for a component to be
// visible it needs to meet all current conditions.
typedef NS_OPTIONS(NSUInteger, ToolbarComponentVisibility) {
  // Default option, the component will never be visible.
  ToolbarComponentVisibilityNone = 0,
  // Use this option when the component should always be visible in
  // CompactWidth.
  ToolbarComponentVisibilityCompactWidth = 1 << 0,
  // Use this option when the component should always be visible in
  // RegularWidth.
  ToolbarComponentVisibilityRegularWidth = 1 << 1,
  // Use this option when the component should be visible in CompactWidth only
  // if it's enabled.
  ToolbarComponentVisibilityCompactWidthOnlyWhenEnabled = 1 << 2,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COMPONENT_OPTIONS_H_
