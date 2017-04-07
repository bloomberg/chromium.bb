// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_

#import "ios/web/public/navigation_manager.h"

// Commands relating to the Settings UI.
@protocol TabCommands
// Open a URL.
- (void)loadURL:(web::NavigationManager::WebLoadParams)params;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
