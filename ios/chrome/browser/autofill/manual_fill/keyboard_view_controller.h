// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_KEYBOARD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_KEYBOARD_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/manual_fill/manual_fill_view_controller.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/keyboard_accessory_view.h"

// Subclass of `ManualFillViewController` with the code that is specific for
// devices with no undocked keyboard.
@interface ManualFillKeyboardViewController
    : ManualFillViewController<KeyboardAccessoryViewDelegate>
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_KEYBOARD_VIEW_CONTROLLER_H_
