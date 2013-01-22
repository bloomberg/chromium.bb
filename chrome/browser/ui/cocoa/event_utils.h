// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EVENT_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_EVENT_UTILS_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/window_open_disposition.h"

namespace event_utils {

// Retrieves a bitsum of ui::EventFlags represented by |event|,
int EventFlagsFromNSEvent(NSEvent* event);

// Retrieves a bitsum of ui::EventFlags represented by |event|,
// but instead use the modifier flags given by |modifiers|,
// which is the same format as |-NSEvent modifierFlags|. This allows
// substitution of the modifiers without having to create a new event from
// scratch.
int EventFlagsFromNSEventWithModifiers(NSEvent* event, NSUInteger modifiers);

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|. For example, a Cmd+Click would mean open the
// associated link in a background tab.
WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event);

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|, but instead use the modifier flags given by |flags|,
// which is the same format as |-NSEvent modifierFlags|. This allows
// substitution of the modifiers without having to create a new event from
// scratch.
WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event, NSUInteger flags);

}  // namespace event_utils

#endif  // CHROME_BROWSER_UI_COCOA_EVENT_UTILS_H_
