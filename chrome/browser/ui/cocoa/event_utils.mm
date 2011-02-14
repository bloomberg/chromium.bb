// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/event_utils.h"

#include "chrome/browser/disposition_utils.h"

namespace event_utils {

WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event) {
  NSUInteger modifiers = [event modifierFlags];
  return WindowOpenDispositionFromNSEventWithFlags(event, modifiers);
}

WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event, NSUInteger flags) {
  return disposition_utils::DispositionFromClick(
      [event buttonNumber] == 2,
      flags & NSAlternateKeyMask,
      flags & NSControlKeyMask,
      flags & NSCommandKeyMask,
      flags & NSShiftKeyMask);
}

}  // namespace event_utils
