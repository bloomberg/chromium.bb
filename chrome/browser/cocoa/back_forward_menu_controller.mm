// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/back_forward_menu_controller.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/back_forward_menu_model.h"
#import "chrome/browser/cocoa/delayedmenu_button.h"
#include "skia/ext/skia_utils_mac.h"

using base::SysUTF16ToNSString;
using gfx::SkBitmapToNSImage;

@implementation BackForwardMenuController

// Accessors and mutators:

@synthesize type = type_;

// Own methods:

- (id)initWithBrowser:(Browser*)browser
            modelType:(BackForwardMenuType)type
               button:(DelayedMenuButton*)button {
  if ((self = [super init])) {
    type_ = type;
    button_ = button;
    model_.reset(new BackForwardMenuModel(browser, type_));
    DCHECK(model_.get());
    backForwardMenu_.reset([[NSMenu alloc] initWithTitle:@""]);
    DCHECK(backForwardMenu_.get());
    [backForwardMenu_ setDelegate:self];

    [button_ setAttachedMenu:backForwardMenu_];
    [button_ setAttachedMenuEnabled:YES];
  }
  return self;
}

// Methods as delegate:

// Called by backForwardMenu_ just before tracking begins.
//TODO(viettrungluu): should we do anything for chapter stops (see model)?
- (void)menuNeedsUpdate:(NSMenu*)menu {
  DCHECK(menu == backForwardMenu_);

  // Remove old menu items (backwards order is as good as any).
  for (NSInteger i = [menu numberOfItems]; i > 0; i--)
    [menu removeItemAtIndex:(i-1)];

  // 0-th item must be blank. (This is because we use a pulldown list, for which
  // Cocoa uses the 0-th item as "title" in the button.)
  [menu insertItemWithTitle:@""
                     action:nil
              keyEquivalent:@""
                    atIndex:0];
  for (int menuID = 1; menuID <= model_->GetTotalItemCount(); menuID++) {
    if (model_->IsSeparator(menuID)) {
      [menu insertItem:[NSMenuItem separatorItem]
               atIndex:menuID];
    } else {
      // Create a menu item with the right label.
      NSMenuItem* menuItem = [[NSMenuItem alloc]
              initWithTitle:SysUTF16ToNSString(model_->GetItemLabel(menuID))
                     action:nil
              keyEquivalent:@""];
      [menuItem autorelease];

      // Only enable it if it's supposed to do something.
      [menuItem setEnabled:(model_->ItemHasCommand(menuID) ? YES : NO)];

      // Icon (if it has one).
      if (model_->ItemHasIcon(menuID))
        [menuItem setImage:SkBitmapToNSImage(model_->GetItemIcon(menuID))];

      // This will make it call our |-executeMenuItem:| method. We store the
      // |menuID| (or |menu_id|) in the tag.
      [menuItem setTag:menuID];
      [menuItem setTarget:self];
      [menuItem setAction:@selector(executeMenuItem:)];

      // Put it in the menu!
      [menu insertItem:menuItem
               atIndex:menuID];
    }
  }
}

// Action methods:

- (void)executeMenuItem:(id)sender {
  DCHECK([sender isKindOfClass:[NSMenuItem class]]);
  int menuID = [sender tag];
  model_->ExecuteCommandById(menuID);
}

@end  // @implementation BackForwardMenuController
