// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_HISTORY_POPUP_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_HISTORY_POPUP_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace web {
class NavigationItem;
}

@protocol TabHistoryPopupCommands
// Shows the tab history popup containing the tab's backward history.
- (void)showTabHistoryPopupForBackwardHistory;

// Shows the tab history popup containing the tab's forward history.
- (void)showTabHistoryPopupForForwardHistory;

// Navigate back/forward to the selected entry in the tab's history.
- (void)navigateToHistoryItem:(const web::NavigationItem*)item;
@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_HISTORY_POPUP_COMMANDS_H_
