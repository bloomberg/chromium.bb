// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_MENU_BUTTON_H_
#define CHROME_BROWSER_COCOA_MENU_BUTTON_H_

#import <Cocoa/Cocoa.h>

// This a button which displays a user-provided menu "attached" below it upon
// being clicked or dragged (or clicked and held). It expects a
// |ClickHoldButtonCell| as cell.
@interface MenuButton : NSButton {
 @private
  IBOutlet NSMenu* menu_;
  BOOL openAtRight_;
}

// The menu to display.
@property(assign, nonatomic) NSMenu* menu;

@end  // @interface MenuButton

#endif  // CHROME_BROWSER_COCOA_MENU_BUTTON_H_
