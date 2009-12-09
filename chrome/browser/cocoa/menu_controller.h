// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_COCOA_MENU_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

namespace menus {
class MenuModel;
}

// A controller for the cross-platform menu model. The menu that's created
// has the tag and represented object set for each menu item. The object is a
// NSValue holding a pointer to the model for that level of the menu (to
// allow for hierarchical menus). The tag is the index into that model for
// that particular item. It is important that the model outlives this object
// as it only maintains weak references.
// TODO(pinkerton): Handle changes to the model. SimpleMenuModel doesn't yet
// notify when changes are made.
@interface MenuController : NSObject {
 @private
  scoped_nsobject<NSMenu> menu_;
  BOOL useWithPopUpButtonCell_;  // If YES, 0th item is blank
}

// Builds a NSMenu from the pre-built model (must not be nil). Changes made
// to the contents of the model after calling this will not be noticed. If
// the menu will be displayed by a NSPopUpButtonCell, it needs to be of a
// slightly different form (0th item is empty). Note this attribute of the menu
// cannot be changed after it has been created.
- (id)initWithModel:(menus::MenuModel*)model
    useWithPopUpButtonCell:(BOOL)useWithCell;

// Access to the constructed menu.
- (NSMenu*)menu;

@end

// Exposed only for unit testing, do not call directly.
@interface MenuController(PrivateExposedForTesting)
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item;
@end

#endif  // CHROME_BROWSER_COCOA_MENU_CONTROLLER_H_
