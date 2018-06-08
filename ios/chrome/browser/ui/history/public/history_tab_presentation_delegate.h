// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_PUBLIC_HISTORY_TAB_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_PUBLIC_HISTORY_TAB_PRESENTATION_DELEGATE_H_

// Delegate used to make the tab UI visible.
@protocol HistoryTabPresentationDelegate
// Tells the delegate to show the non-incognito tab UI. NO-OP if the correct tab
// UI is already visible.
- (void)showActiveRegularTab;
// Tells the delegate to show the incognito tab UI. NO-OP if the correct tab UI
// is already visible.
- (void)showActiveIncognitoTab;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_PUBLIC_HISTORY_TAB_PRESENTATION_DELEGATE_H_
