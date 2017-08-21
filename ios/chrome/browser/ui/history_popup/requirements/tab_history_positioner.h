// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_POPUP_REQUIREMENTS_TAB_HISTORY_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_POPUP_REQUIREMENTS_TAB_HISTORY_POSITIONER_H_

#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"

@protocol TabHistoryPositioner
// CGPoint which the Tab History Popup will be presented from.
- (CGPoint)originPointForToolbarButton:(ToolbarButtonType)toolbarButton;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_POPUP_REQUIREMENTS_TAB_HISTORY_POSITIONER_H_
