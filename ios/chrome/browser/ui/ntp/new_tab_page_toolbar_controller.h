// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_TOOLBAR_CONTROLLER_H_

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

@protocol OmniboxFocuser;
@protocol WebToolbarDelegate;

// New tab page specific toolbar. The background view is hidden and the
// navigation buttons are also hidden if there is no forward history. Does not
// contain an omnibox but tapping in the center will focus the main toolbar's
// omnibox.
@interface NewTabPageToolbarController : ToolbarController

// Designated initializer. The underlying ToolbarController is initialized with
// ToolbarControllerStyleLightMode.
- (instancetype)initWithToolbarDelegate:(id<WebToolbarDelegate>)delegate
                                focuser:(id<OmniboxFocuser>)focuser;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_TOOLBAR_CONTROLLER_H_
