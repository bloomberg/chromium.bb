// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Used to enable the UI Refresh omnibox popup presentation. This flag should
// not be used directly. Instead use
// ui_util::IsRefreshPopupPresentationEnabled().
extern const base::Feature kRefreshPopupPresentation;

// Used to enable the UI Refresh location bar/omnibox. This flag should
// not be used directly. Instead use
// ui_util::IsRefreshLocationBarEnabled().
extern const base::Feature kUIRefreshLocationBar;

// Used to enable the first phase of the UI refresh. This flag should not be
// used directly. Instead use ui_util::IsUIRefreshPhase1Enabled().
extern const base::Feature kUIRefreshPhase1;

// Feature to choose whether to use the new UI Reboot Infobar UX, or the legacy
// one. This flag should not be used directly. Instead, use
// IsRefreshInfobarEnabled().
extern const base::Feature kInfobarsUIReboot;

// Avoid the crash in https://crbug.com/816427 by getting the first responder by
// navigating the key window, rather than using -sendAction to find the first
// responder.
extern const base::Feature kFirstResponderKeyWindow;

// Feature to copy image to system pasteboard via context menu.
extern const base::Feature kCopyImage;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
