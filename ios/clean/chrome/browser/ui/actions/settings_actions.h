// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_

#import <Foundation/Foundation.h>
// HACK: This whole file and the two remaining action calls need to
// disappear. We just need to decide how VC's will deal with tap/actions that
// were set by the mediator or some other file. Target/Action methods relating
// to the Settings UI. (Actions should only be used to communicate into or
// between the View Controller layer).
@protocol SettingsActions
@optional
// Show the Settings UI over whatever UI is currently active.
- (void)showSettings:(id)sender;
// Shows the ToolsMenu from the TabGrid button. This way we can access settings.
- (void)showToolsMenu:(id)sender;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_SETTINGS_ACTIONS_H_
