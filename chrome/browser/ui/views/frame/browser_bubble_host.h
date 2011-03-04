// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_BUBBLE_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_BUBBLE_HOST_H_
#pragma once

#include <set>

#include "base/basictypes.h"

class BrowserBubble;

// A class providing a hosting environment for BrowserBubble instances.
// Allows for notification to attached BrowserBubbles of browser move, and
// close events.
class BrowserBubbleHost {
 public:
  BrowserBubbleHost();
  ~BrowserBubbleHost();

  // Invoked when the window containing the attached browser-bubbles is moved.
  // Calls BrowserBubble::BrowserWindowMoved on all attached bubbles.
  void WindowMoved();

  // To be called when the frame containing the BrowserBubbleHost is closing.
  // Calls BrowserBubble::BrowserWindowClosing on all attached bubbles.
  void Close();

  // Registers/Unregisters |bubble| to receive notifications when the host moves
  // or is closed.
  void AttachBrowserBubble(BrowserBubble* bubble);
  void DetachBrowserBubble(BrowserBubble* bubble);

 private:
  // The set of bubbles associated with this host.
  typedef std::set<BrowserBubble*> BubbleSet;
  BubbleSet browser_bubbles_;

  DISALLOW_COPY_AND_ASSIGN(BrowserBubbleHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_BUBBLE_HOST_H_
