// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NAVIGATION_GESTURE_H_
#define CHROME_COMMON_NAVIGATION_GESTURE_H_
#pragma once

enum NavigationGesture {
  NavigationGestureUser,    // User initiated navigation/load. This is not
                            // currently used due to the untrustworthy nature
                            // of userGestureHint (wasRunByUserGesture). See
                            // bug 1051891.
  NavigationGestureAuto,    // Non-user initiated navigation / load. For example
                            // onload or setTimeout triggered document.location
                            // changes, and form.submits. See bug 1046841 for
                            // some cases that should be treated this way but
                            // aren't yet.
  NavigationGestureUnknown, // What we assign when userGestureHint returns true
                            // because we can't trust it.
};

#endif  // CHROME_COMMON_NAVIGATION_GESTURE_H_
