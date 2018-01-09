// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_TOOLBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_TOOLBAR_COMMANDS_H_

// Protocol that describes the commands that trigger Toolbar UI changes.
@protocol ToolbarCommands
// Contracts the Toolbar to its regular form.
- (void)contractToolbar;

// Triggers the animation of the tools menu button.
- (void)triggerToolsMenuButtonAnimation;

// Navigates to the Memex tab switcher.
// TODO(crbug.com/799601): Delete this once its not needed.
- (void)navigateToMemexTabSwitcher;
@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_TOOLBAR_COMMANDS_H_
