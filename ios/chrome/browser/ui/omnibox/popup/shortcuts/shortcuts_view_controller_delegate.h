// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_DELEGATE_H_

@class ShortcutsMostVisitedItem;

// Commands originating from ShortcutsViewController, for example ones called
// when the shortcuts are tapped.
@protocol ShortcutsViewControllerDelegate

// Called when a most visited shortcut is selected by the user.
- (void)openMostVisitedItem:(ShortcutsMostVisitedItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_DELEGATE_H_
