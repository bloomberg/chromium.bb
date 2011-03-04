// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_bubble_host.h"

#include "base/logging.h"
#include "chrome/browser/ui/views/browser_bubble.h"

BrowserBubbleHost::BrowserBubbleHost() {}

BrowserBubbleHost::~BrowserBubbleHost() {}

void BrowserBubbleHost::WindowMoved() {
  // Do safe iteration in case the bubble winds up closing as a result of this
  // message.
  for (BubbleSet::iterator i = browser_bubbles_.begin();
       i != browser_bubbles_.end();) {
    BubbleSet::iterator bubble = i++;
    (*bubble)->BrowserWindowMoved();
  }
}

void BrowserBubbleHost::AttachBrowserBubble(BrowserBubble* bubble) {
  DCHECK(browser_bubbles_.find(bubble) == browser_bubbles_.end()) <<
      "Attempt to register the same BrowserBubble multiple times.";
  browser_bubbles_.insert(bubble);
}

void BrowserBubbleHost::DetachBrowserBubble(BrowserBubble* bubble) {
  BubbleSet::iterator it = browser_bubbles_.find(bubble);
  DCHECK(it != browser_bubbles_.end()) <<
      "Attempt to detach an unrecognized BrowserBubble.";
  if (it != browser_bubbles_.end())
    browser_bubbles_.erase(it);
}

void BrowserBubbleHost::Close() {
  // BrowserWindowClosing will usually cause the bubble to remove itself from
  // the set, so we need to iterate in a way that's safe against deletion.
  for (BubbleSet::iterator i = browser_bubbles_.begin();
       i != browser_bubbles_.end();) {
    BubbleSet::iterator bubble = i++;
    (*bubble)->BrowserWindowClosing();
  }
}

