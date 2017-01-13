// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEY_COMMANDS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_KEY_COMMANDS_PROVIDER_H_

#import <UIKit/UIKit.h>

@protocol KeyCommandsPlumbing<NSObject>

#pragma mark Query information

// Whether the current profile is off-the-record.
- (BOOL)isOffTheRecord;

// Returns the current number of tabs.
- (NSUInteger)tabsCount;

// Whether navigation to the previous page is available.
- (BOOL)canGoBack;

// Whether navigation to the next page is available.
- (BOOL)canGoForward;

#pragma mark Call for action

// Executes a Chrome command.  |sender| must implement the |-tag| method and
// return the id of the command to execute.  See UIKit+ChromeExecuteCommand.h
// for more details.
- (void)chromeExecuteCommand:(id)sender;

// Called to put the tab at index in focus.
- (void)focusTabAtIndex:(NSUInteger)index;

// Called to focus the next tab.
- (void)focusNextTab;

// Called to focus the previous tab.
- (void)focusPreviousTab;

// Called to reopen the last closed tab.
- (void)reopenClosedTab;

// Called to focus the omnibox.
- (void)focusOmnibox;

@end

// Handles the keyboard commands registration and handling for the
// BrowserViewController.
@interface KeyCommandsProvider : NSObject

- (NSArray*)keyCommandsForConsumer:(id<KeyCommandsPlumbing>)consumer
                       editingText:(BOOL)editingText;

@end

#endif  // IOS_CHROME_BROWSER_UI_KEY_COMMANDS_PROVIDER_H_
