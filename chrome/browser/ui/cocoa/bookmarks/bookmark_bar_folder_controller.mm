// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"

// Forward-declare symbols that are part of the 10.6 SDK.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

@interface NSMenu (SnowLeopardSDK)
- (BOOL)popUpMenuPositioningItem:(NSMenuItem*)item
                      atLocation:(NSPoint)location
                          inView:(NSView*)view;
@end

#endif  // MAC_OS_X_VERSION_10_6

@implementation BookmarkBarFolderController

- (id)initWithParentButton:(BookmarkButton*)button
             bookmarkModel:(BookmarkModel*)model
             barController:(BookmarkBarController*)barController {
  if ((self = [super init])) {
    parentButton_.reset([button retain]);
    barController_ = barController;
    menu_.reset([[NSMenu alloc] initWithTitle:@""]);
    menuBridge_.reset(new BookmarkMenuBridge([parentButton_ bookmarkNode],
        model->profile(), menu_));
    [menuBridge_->controller() setDelegate:self];
  }
  return self;
}

- (BookmarkButton*)parentButton {
  return parentButton_.get();
}

- (void)openMenu {
  // Retain self so that whatever created this can forefit ownership if it
  // wants. This call is balanced in |-bookmarkMenuDidClose:|.
  [self retain];

  // If the system supports opening the menu at a specific point, do so.
  // Otherwise, it will be opened at the mouse event location. Eventually these
  // should be switched to NSPopUpButtonCells so that this is taken care of
  // automatically.
  if ([menu_ respondsToSelector:
          @selector(popUpMenuPositioningItem:atLocation:inView:)]) {
    NSPoint point = [parentButton_ frame].origin;
    point.y -= bookmarks::kBookmarkBarMenuOffset;
    [menu_ popUpMenuPositioningItem:nil
                         atLocation:point
                             inView:[parentButton_ superview]];
  } else {
    [NSMenu popUpContextMenu:menu_
                   withEvent:[NSApp currentEvent]
                     forView:parentButton_];
  }
}

- (void)closeMenu {
  NSArray* modes = [NSArray arrayWithObject:NSRunLoopCommonModes];
  [menu_ performSelector:@selector(cancelTracking)
              withObject:nil
              afterDelay:0.0
                 inModes:modes];
}

- (void)setOffTheSideNodeStartIndex:(size_t)index {
  menuBridge_->set_off_the_side_node_start_index(index);
}

- (void)bookmarkMenuDidClose:(BookmarkMenuCocoaController*)controller {
  // Inform the bookmark bar that the folder has closed on the next iteration
  // of the event loop. If the menu was closed via a click event on a folder
  // button, this message will be received before dispatching the click event
  // to the button. If the button is the same folder button that ran the menu
  // in the first place, this will recursively pop open the menu because the
  // active folder will be nil-ed by |-closeBookmarkFolder:|. To prevent that,
  // perform the selector on the next iteration of the loop.
  [barController_ performSelector:@selector(closeBookmarkFolder:)
                       withObject:self
                       afterDelay:0.0];

  // This controller is created on-demand and should be released when the menu
  // closes because a new one will be created when it is opened again.
  [self autorelease];
}

@end

////////////////////////////////////////////////////////////////////////////////

@implementation BookmarkBarFolderController (ExposedForTesting)

- (BookmarkMenuBridge*)menuBridge {
  return menuBridge_.get();
}

@end
