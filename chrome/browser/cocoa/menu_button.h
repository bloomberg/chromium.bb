// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_MENU_BUTTON_H_
#define CHROME_BROWSER_COCOA_MENU_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// This a button which displays a user-provided menu "attached" below it upon
// being clicked or dragged (or clicked and held). It expects a
// |ClickHoldButtonCell| as cell.
@interface MenuButton : NSButton {
 @private
  IBOutlet NSMenu* attachedMenu_;
  scoped_nsobject<NSPopUpButtonCell> popUpCell_;
}

// The menu to display. Note that it should have no (i.e., a blank) title and
// that the 0-th entry should be blank (and won't be displayed). (This is
// because we use a pulldown list, for which Cocoa uses the 0-th item as "title"
// in the button. This might change if we ever switch to a pop-up. Our direct
// use of the given NSMenu object means that the one can set and use NSMenu's
// delegate as usual.)
@property(assign, nonatomic) NSMenu* attachedMenu;

@end  // @interface MenuButton

#endif  // CHROME_BROWSER_COCOA_MENU_BUTTON_H_
