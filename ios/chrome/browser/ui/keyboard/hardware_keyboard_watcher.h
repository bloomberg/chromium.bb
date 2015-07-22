// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_HARDWARE_KEYBOARD_WATCHER_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_HARDWARE_KEYBOARD_WATCHER_H_

#import <UIKit/UIKit.h>

// Watches keyboard events to determine if the keyboard is software (provided by
// iOS, fully visible on screen when showing) or hardware (external keyboard,
// only showing a potential input accessory view).
// It reports the mode for each keyboard frame change via an UMA histogram
// (Omnibox.HardwareKeyboardModeEnabled).
@interface HardwareKeyboardWatcher : NSObject

// Pass an accessory view to check for presence in the view hierarchy. Keyboard
// presentation/dismissal with no input accessory view have a different code
// path between hardware and software keyboard mode, thus unreliable for
// metrics comparisons.
// |accessoryView| must not be nil.
- (instancetype)initWithAccessoryView:(UIView*)accessoryView
    NS_DESIGNATED_INITIALIZER;

// Detection of external keyboards only works when an input accessory view is
// set.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_HARDWARE_KEYBOARD_WATCHER_H_
