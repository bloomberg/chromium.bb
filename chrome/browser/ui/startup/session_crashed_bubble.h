// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_BUBBLE_H_
#define CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_BUBBLE_H_

class Browser;

// Create a session recovery bubble if the last session crashed. It also offers
// the option to enable metrics reporting if it's not already enabled. Function
// returns true if a bubble is created, returns false if nothing is created.
bool ShowSessionCrashedBubble(Browser* browser);

#endif  // CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_BUBBLE_H_
