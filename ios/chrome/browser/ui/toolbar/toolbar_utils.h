// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UTILS_H_

#import <UIKit/UIKit.h>

// Returns the height of the toolbar when it is collapsed, based on the current
// |category|.
CGFloat ToolbarCollapsedHeight(UIContentSizeCategory category);

// Returns the height of the toolbar when it is expanded, based on the current
// |category|.
CGFloat ToolbarExpandedHeight(UIContentSizeCategory category);

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UTILS_H_
