// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/cocoa_utils.h"

namespace event_utils {

// Translates modifier flags from an NSEvent into a WindowOpenDisposition. For
// example, holding down Cmd (Apple) will cause pages to be opened in a new
// foreground tab. Pass this the result of -[NSEvent modifierFlags].
WindowOpenDisposition DispositionFromEventFlags(NSUInteger modifiers) {
  if (modifiers & NSCommandKeyMask) {
    return (modifiers & NSShiftKeyMask) ?
        NEW_BACKGROUND_TAB : NEW_FOREGROUND_TAB;
  }

  if (modifiers & NSShiftKeyMask)
    return NEW_WINDOW;
  // TODO: Browser::OpenURLAtIndex does not support SAVE_TO_DISK, so we can't
  // offer to download the page. See DCHECK() and TODOs there.
  return (false /* modifiers & NSAlternateKeyMask */) ?
      SAVE_TO_DISK : CURRENT_TAB;
}

}  // namespace event_utils
