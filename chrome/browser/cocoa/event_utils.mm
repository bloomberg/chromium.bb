// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/event_utils.h"

namespace event_utils {

WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event) {
  NSUInteger modifiers = [event modifierFlags];
  if ([event buttonNumber] == 2 || modifiers & NSCommandKeyMask)
    return modifiers & NSShiftKeyMask ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  return modifiers & NSShiftKeyMask ? NEW_WINDOW : CURRENT_TAB;
}

}  // namespace event_utils
