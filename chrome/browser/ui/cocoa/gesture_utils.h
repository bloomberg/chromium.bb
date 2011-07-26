// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_GESTURE_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_GESTURE_UTILS_H_

#include <Foundation/Foundation.h>

namespace gesture_utils {

// On Lion, Apple introduced a new gesture for navigating pages using two-finger
// gestures. Returns YES if two-finger gestures should be recognized.
BOOL RecognizeTwoFingerGestures();

// On Lion, returns YES if the scroll direction is "natural"/inverted. All other
// OSes will return NO.
BOOL IsScrollDirectionInverted();

}  // namespace gesture_utils

#endif  // CHROME_BROWSER_UI_COCOA_GESTURE_UTILS_H_
