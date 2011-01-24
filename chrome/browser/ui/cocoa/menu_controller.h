// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_MENU_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_nsobject.h"

namespace ui {
class MenuModel;
}

// A controller for the cross-platform menu model. The menu that's created
// has the tag and represented object set for each menu item. The object is a
// NSValue holding a pointer to the model for that level of the menu (to
// allow for hierarchical menus). The tag is the index into that model for
// that particular item. It is important that the model outlives this object
// as it only maintains weak references.
@interface MenuController : NSObject<NSMenuDelegate> {
 @protected
  ui::MenuModel* model_;  // weak
  scoped_nsobject<NSMenu> menu_;
  BOOL useWithPopUpButtonCell_;  // If YES, 0th item is blank
}

@property(nonatomic, assign) ui::MenuModel* model;
// Note that changing this will have no effect if you use
// |-initWithModel:useWithPopUpButtonCell:| or after the first call to |-menu|.
@property(nonatomic) BOOL useWithPopUpButtonCell;

// NIB-based initializer. This does not create a menu. Clients can set the
// properties of the object and the menu will be created upon the first call to
// |-menu|. Note that the menu will be immutable after creation.
- (id)init;

// Builds a NSMenu from the pre-built model (must not be nil). Changes made
// to the contents of the model after calling this will not be noticed. If
// the menu will be displayed by a NSPopUpButtonCell, it needs to be of a
// slightly different form (0th item is empty). Note this attribute of the menu
// cannot be changed after it has been created.
- (id)initWithModel:(ui::MenuModel*)model
    useWithPopUpButtonCell:(BOOL)useWithCell;

// Access to the constructed menu if the complex initializer was used. If the
// default initializer was used, then this will create the menu on first call.
- (NSMenu*)menu;

@end

// Exposed only for unit testing, do not call directly.
@interface MenuController (PrivateExposedForTesting)
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item;
@end

// Protected methods that subclassers can override.
@interface MenuController (Protected)
- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model
           modelIndex:(int)modelIndex;
@end

#endif  // CHROME_BROWSER_UI_COCOA_MENU_CONTROLLER_H_
