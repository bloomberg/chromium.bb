// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Controller (MVC) for the history menu. All history menu item commands get
// directed here. This class only responds to menu events, but the actual
// creation and maintenance of the menu happens in the Bridge.

#ifndef CHROME_BROWSER_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/history_menu_bridge.h"

@interface HistoryMenuCocoaController : NSObject {
 @private
  HistoryMenuBridge* bridge_;  // weak; owns us
}

- (id)initWithBridge:(HistoryMenuBridge*)bridge;

// Called by any history menu item.
- (IBAction)openHistoryMenuItem:(id)sender;

@end  // HistoryMenuCocoaController

@interface HistoryMenuCocoaController (ExposedForUnitTests)
- (HistoryMenuBridge::HistoryItem)itemForTag:(NSInteger)tag;
- (void)openURLForItem:(HistoryMenuBridge::HistoryItem&)node;
@end  // HistoryMenuCocoaController (ExposedForUnitTests)

#endif  // CHROME_BROWSER_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_
