// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_VIEWS_H_
#define CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_VIEWS_H_

namespace views {
class BubbleDialogDelegateView;
}

// Monitors |bubble|'s parent window for size changes, and updates the bubble
// anchor. The monitor will be deleted when |bubble| is closed.
void KeepBubbleAnchored(views::BubbleDialogDelegateView* bubble);

#endif  // CHROME_BROWSER_UI_COCOA_BUBBLE_ANCHOR_HELPER_VIEWS_H_
