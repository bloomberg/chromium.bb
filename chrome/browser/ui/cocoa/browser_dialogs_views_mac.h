// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_DIALOGS_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_DIALOGS_VIEWS_MAC_H_

namespace chrome {

// Closes a views zoom bubble if currently shown.
void CloseZoomBubbleViews();

// Refreshes views zoom bubble if currently shown.
void RefreshZoomBubbleViews();

// Returns true if views zoom bubble is currently shown.
bool IsZoomBubbleViewsShown();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_DIALOGS_VIEWS_MAC_H_
