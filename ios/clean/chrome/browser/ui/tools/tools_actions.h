// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_ACTIONS_H_

// Target/Action methods related to the tools menu.
@protocol ToolsActions
@optional

// Shows the UI for Find in Page.
- (void)showFindInPage;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_ACTIONS_H_
