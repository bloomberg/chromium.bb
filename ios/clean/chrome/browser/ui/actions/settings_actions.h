// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_

#import <Foundation/Foundation.h>

// Target/Action methods relating to the Settings UI.
// (Actions should only be used to communicate into or between the View
// Controller layer).
@protocol SettingsActions
@optional
// Show the Settings UI over whatever UI is currently active.
- (void)showSettings:(id)sender;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_
