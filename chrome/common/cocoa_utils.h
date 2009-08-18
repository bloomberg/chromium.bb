// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COCOA_UTIL_H_
#define CHROME_COMMON_COCOA_UTIL_H_

#import <Cocoa/Cocoa.h>
#include "webkit/glue/window_open_disposition.h"

namespace event_utils {

// Translates modifier flags from an NSEvent into a WindowOpenDisposition. For
// example, holding down Cmd (Apple) will cause pages to be opened in a new
// foreground tab. Pass this the result of -[NSEvent modifierFlags].
WindowOpenDisposition DispositionFromEventFlags(NSUInteger modifiers);

}  // namespace event_utils

#endif  // CHROME_COMMON_COCOA_UTIL_H_
