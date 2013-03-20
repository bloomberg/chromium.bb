// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_context_menu_cocoa_controller.h"

#include <vector>

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/browser.h"

@interface BookmarkContextMenuCocoaController (Private)
- (void)willExecuteCommand;
@end

class BookmarkContextMenuDelegateBridge :
    public BookmarkContextMenuControllerDelegate {
 public:
  BookmarkContextMenuDelegateBridge(
      BookmarkContextMenuCocoaController* controller)
      : controller_(controller) {
  }

  virtual ~BookmarkContextMenuDelegateBridge() {
  }

  virtual void CloseMenu() OVERRIDE {
    [controller_ cancelTracking];
  }

  virtual void WillExecuteCommand(
      int command_id,
      const std::vector<const BookmarkNode*>& bookmarks) OVERRIDE {
    [controller_ willExecuteCommand];
  }

 private:
  BookmarkContextMenuCocoaController* controller_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuDelegateBridge);
};

@implementation BookmarkContextMenuCocoaController

- (id)initWithBookmarkBarController:(BookmarkBarController*)controller {
  if ((self = [super init])) {
    bookmarkBarController_ = controller;
    bridge_.reset(new BookmarkContextMenuDelegateBridge(self));
  }
  return self;
}

// Re-creates |bookmarkContextMenuController_| and |menuController_| based on
// |bookmarkModel| and the current value of |bookmarkNode_|.
- (void)createMenuControllers:(BookmarkModel*)bookmarkModel {
  const BookmarkNode* parent = NULL;
  std::vector<const BookmarkNode*> nodes;
  if (bookmarkNode_ == bookmarkModel->other_node()) {
    nodes.push_back(bookmarkNode_);
    parent = bookmarkModel->bookmark_bar_node();
  } else if (bookmarkNode_ != bookmarkModel->bookmark_bar_node()) {
    nodes.push_back(bookmarkNode_);
    parent = bookmarkNode_->parent();
  } else {
    parent = bookmarkModel->bookmark_bar_node();
    nodes.push_back(parent);
  }

  Browser* browser = [bookmarkBarController_ browser];
  bookmarkContextMenuController_.reset(
      new BookmarkContextMenuController([bookmarkBarController_ browserWindow],
                                        bridge_.get(), browser,
                                        browser->profile(), browser, parent,
                                        nodes));
  ui::SimpleMenuModel* menuModel =
      bookmarkContextMenuController_->menu_model();
  menuController_.reset([[MenuController alloc] initWithModel:menuModel
                                       useWithPopUpButtonCell:NO]);
}

- (NSMenu*)menuForBookmarkNode:(const BookmarkNode*)node {
  // Depending on timing, the model may not yet have been loaded.
  BookmarkModel* bookmarkModel = [bookmarkBarController_ bookmarkModel];
  if (!bookmarkModel || !bookmarkModel->IsLoaded())
    return nil;

  if (!node)
    node = bookmarkModel->bookmark_bar_node();

  // Rebuild the menu if it's for a different node than the last request.
  if (!menuController_ || node != bookmarkNode_) {
    bookmarkNode_ = node;
    [self createMenuControllers:bookmarkModel];
  }
  return [menuController_ menu];
}

- (void)willExecuteCommand {
  [bookmarkBarController_ closeFolderAndStopTrackingMenus];
  [bookmarkBarController_ unhighlightBookmark:bookmarkNode_];
}

- (void)cancelTracking {
  [[menuController_ menu] cancelTracking];
}

@end
