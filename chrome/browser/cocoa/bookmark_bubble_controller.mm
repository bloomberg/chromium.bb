// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#import "chrome/browser/cocoa/bookmark_bubble_window.h"
#include "grit/generated_resources.h"


@interface BookmarkBubbleController(PrivateAPI)
- (void)closeWindow;
@end

@implementation BookmarkBubbleController

@synthesize delegate = delegate_;
@synthesize folderComboBox = folderComboBox_;

- (id)initWithDelegate:(id<BookmarkBubbleControllerDelegate>)delegate
          parentWindow:(NSWindow*)parentWindow
      topLeftForBubble:(NSPoint)topLeftForBubble
                 model:(BookmarkModel*)model
                  node:(const BookmarkNode*)node
     alreadyBookmarked:(BOOL)alreadyBookmarked {
  if ((self = [super initWithNibName:@"BookmarkBubble"
                              bundle:mac_util::MainAppBundle()])) {
    // all these are weak...
    delegate_ = delegate;
    parentWindow_ = parentWindow;
    topLeftForBubble_ = topLeftForBubble;
    model_ = model;
    node_ = node;
    alreadyBookmarked_ = alreadyBookmarked;
    // But this is strong.
    titleMapping_.reset([[NSMutableDictionary alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self closeWindow];
  [super dealloc];
}

- (void)showWindow {
  [self view];  // force nib load and window_ allocation
  [window_ makeKeyAndOrderFront:self];
}

// Actually close the window.  Do nothing else.
- (void)closeWindow {
  [parentWindow_ removeChildWindow:window_];
  [window_ close];
}

- (void)awakeFromNib {
  window_.reset([self createBubbleWindow]);
  [parentWindow_ addChildWindow:window_ ordered:NSWindowAbove];

  // Fill in inital values for text, controls, ...

  // Default is IDS_BOOMARK_BUBBLE_PAGE_BOOKMARK; "Bookmark".
  // If adding for the 1st time the string becomes "Bookmark Added!"
  if (!alreadyBookmarked_) {
    NSString* title =
      l10n_util::GetNSString(IDS_BOOMARK_BUBBLE_PAGE_BOOKMARKED);
    [bigTitle_ setStringValue:title];
  }

  [self fillInFolderList];
}

- (IBAction)edit:(id)sender {
  [self updateBookmarkNode];
  [self closeWindow];
  [delegate_ editBookmarkNode:node_];
  [delegate_ doneWithBubbleController:self];
}

- (IBAction)close:(id)sender {
  if (node_) {
    // no node_ if the bookmark was just removed
    [self updateBookmarkNode];
  }
  [self closeWindow];
  [delegate_ doneWithBubbleController:self];
}

// By implementing this, ESC causes the window to go away.
- (IBAction)cancel:(id)sender {
  [self close:sender];
}

- (IBAction)remove:(id)sender {
  model_->SetURLStarred(node_->GetURL(), node_->GetTitle(), false);
  node_ = NULL;  // no longer valid
  [self close:self];
}

// We are the delegate of the combo box so we can tell when "choose
// another folder" was picked.
- (void)comboBoxSelectionDidChange:(NSNotification*)notification {
  NSString* selected = [folderComboBox_ objectValueOfSelectedItem];
  if ([selected isEqual:chooseAnotherFolder_.get()]) {
    [self edit:self];
  }
}

// We are the delegate of our own window so we know when we lose key.
// When we lose key status we close, mirroring Windows behaivor.
- (void)windowDidResignKey:(NSNotification*)notification {

  // If we get here, we are done with this window and controller.  The
  // call of close: may destroy us which destroys the window.  But the
  // window is in the middle of processing resignKeyWindow.  We
  // retain/autorelease the window to insure it lasts until the end of
  // this event.
  [[window_ retain] autorelease];

  if ([window_ isVisible])
    [self close:self];
}

@end  // BookmarkBubbleController


@implementation BookmarkBubbleController(ExposedForUnitTesting)

// Create and return a retained NSWindow for this bubble.
- (NSWindow*)createBubbleWindow {
  NSRect contentRect = [[self view] frame];
  NSPoint origin = topLeftForBubble_;
  origin.y -= contentRect.size.height;  // since it'll be our bottom-left
  contentRect.origin = origin;
  // Now convert to global coordinates since it'll be used for a window.
  contentRect.origin = [parentWindow_ convertBaseToScreen:contentRect.origin];
  NSWindow* window = [[BookmarkBubbleWindow alloc]
                         initWithContentRect:contentRect];
  [window setDelegate:self];
  [window setContentView:[self view]];
  return window;
}

// Fill in all information related to the folder combo box.
//
// TODO(jrg): make sure nested folders that have the same name are
// handled properly.
// http://crbug.com/19408
- (void)fillInFolderList {
  [nameTextField_ setStringValue:base::SysWideToNSString(node_->GetTitle())];
  [self addFolderNodes:model_->root_node() toComboBox:folderComboBox_];

  // Add "Choose another folder...".  Remember it for later to compare against.
  chooseAnotherFolder_.reset(
    [l10n_util::GetNSStringWithFixup(IDS_BOOMARK_BUBBLE_CHOOSER_ANOTHER_FOLDER)
              retain]);
  [folderComboBox_ addItemWithObjectValue:chooseAnotherFolder_.get()];

  // Finally, select the current parent.
  NSString* parentTitle = base::SysWideToNSString(
    node_->GetParent()->GetTitle());
  [folderComboBox_ selectItemWithObjectValue:parentTitle];
}

- (BOOL)windowHasBeenClosed {
  return ![window_ isVisible];
}

// For the given folder node, walk the tree and add folder names to
// the given combo box.
//
// TODO(jrg): no distinction is made among folders with the same name.
- (void)addFolderNodes:(const BookmarkNode*)parent toComboBox:(NSComboBox*)box {
  NSString* title = base::SysWideToNSString(parent->GetTitle());
  if ([title length])  {  // no title if root
    [box addItemWithObjectValue:title];
    [titleMapping_ setValue:[NSValue valueWithPointer:parent] forKey:title];
  }
  for (int i = 0; i < parent->GetChildCount(); i++) {
    const BookmarkNode* child = parent->GetChild(i);
    if (child->is_folder())
      [self addFolderNodes:child toComboBox:box];
  }
}

// Look at the dialog; if the user has changed anything, update the
// bookmark node to reflect this.
- (void)updateBookmarkNode {
  // First the title...
  NSString* oldTitle = base::SysWideToNSString(node_->GetTitle());
  NSString* newTitle = [nameTextField_ stringValue];
  if (![oldTitle isEqual:newTitle]) {
    model_->SetTitle(node_, base::SysNSStringToWide(newTitle));
  }
  // Then the parent folder.
  NSString* oldParentTitle = base::SysWideToNSString(
    node_->GetParent()->GetTitle());
  NSString* newParentTitle = [folderComboBox_ objectValueOfSelectedItem];
  if (![oldParentTitle isEqual:newParentTitle]) {
    const BookmarkNode* newParent = static_cast<const BookmarkNode*>(
      [[titleMapping_ objectForKey:newParentTitle] pointerValue]);
    if (newParent) {
      // newParent should only ever possibly be NULL in a unit test.
      int index = newParent->GetChildCount();
      model_->Move(node_, newParent, index);
    }
  }
}

- (void)setTitle:(NSString*)title parentFolder:(NSString*)folder {
  [nameTextField_ setStringValue:title];
  [folderComboBox_ selectItemWithObjectValue:folder];
}

- (NSString*)chooseAnotherFolderString {
  return chooseAnotherFolder_.get();
}

@end  // implementation BookmarkBubbleController(ExposedForUnitTesting)
