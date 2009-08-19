// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EVENT_UTILS_H_
#define CHROME_BROWSER_COCOA_EVENT_UTILS_H_

#import <Cocoa/Cocoa.h>

#include "webkit/glue/window_open_disposition.h"

namespace event_utils {

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|. For example, a Cmd+Click would mean open the
// associated link in a background tab.
WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event);

}  // namespace event_utils

#endif  // CHROME_BROWSER_COCOA_EVENT_UTILS_H_
