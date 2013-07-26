// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/menu_controller.h"
#include "base/mac/scoped_nsobject.h"

@class DownloadItemController;

// MenuController that retains a DownloadItemController and instantiates a menu
// based on the contextMenuModel from the DownloadItemController.
@interface DownloadShelfContextMenuController : MenuController {
 @private
  base::scoped_nsobject<DownloadItemController> itemController_;
  id<NSMenuDelegate> menuDelegate_;
}

// Initializes the MenuController using [itemController contextMenuModel].
// menuDelegate will be sent a menuDidClose message when the menu closes.
- (id)initWithItemController:(DownloadItemController*)itemController
                withDelegate:(id<NSMenuDelegate>)menuDelegate;

- (void)menuDidClose:(NSMenu*)menu;
@end
