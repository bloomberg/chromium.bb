// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/back_forward_menu_controller.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::SysUTF16ToNSString;
using gfx::SkBitmapToNSImage;

@implementation BackForwardMenuController

// Accessors and mutators:

@synthesize type = type_;

// Own methods:

- (id)initWithBrowser:(Browser*)browser
            modelType:(BackForwardMenuType)type
               button:(MenuButton*)button {
  if ((self = [super init])) {
    type_ = type;
    button_ = button;
    model_.reset(new BackForwardMenuModel(browser, type_));
    DCHECK(model_.get());
    backForwardMenu_.reset([[NSMenu alloc] initWithTitle:@""]);
    DCHECK(backForwardMenu_.get());
    [backForwardMenu_ setDelegate:self];

    [button_ setAttachedMenu:backForwardMenu_];
    [button_ setOpenMenuOnClick:NO];
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
    [menu removeItemAtIndex:(i - 1)];

  // 0-th item must be blank. (This is because we use a pulldown list, for which
  // Cocoa uses the 0-th item as "title" in the button.)
  [menu insertItemWithTitle:@""
                     action:nil
              keyEquivalent:@""
                    atIndex:0];
  for (int menuID = 0; menuID < model_->GetItemCount(); menuID++) {
    if (model_->IsSeparator(menuID)) {
      [menu insertItem:[NSMenuItem separatorItem]
               atIndex:(menuID + 1)];
    } else {
      // Create a menu item with the right label.
      NSMenuItem* menuItem = [[NSMenuItem alloc]
              initWithTitle:SysUTF16ToNSString(model_->GetLabelAt(menuID))
                     action:nil
              keyEquivalent:@""];
      [menuItem autorelease];

      SkBitmap icon;
      // Icon (if it has one).
      if (model_->GetIconAt(menuID, &icon))
        [menuItem setImage:SkBitmapToNSImage(icon)];

      // This will make it call our |-executeMenuItem:| method. We store the
      // |menuID| (or |menu_id|) in the tag.
      [menuItem setTag:menuID];
      [menuItem setTarget:self];
      [menuItem setAction:@selector(executeMenuItem:)];

      // Put it in the menu!
      [menu insertItem:menuItem
               atIndex:(menuID + 1)];
    }
  }
}

// Action methods:

- (void)executeMenuItem:(id)sender {
  DCHECK([sender isKindOfClass:[NSMenuItem class]]);
  int menuID = [sender tag];
  model_->ActivatedAtWithDisposition(
      menuID,
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]));
}

@end  // @implementation BackForwardMenuController
