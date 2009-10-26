// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#import "chrome/browser/cocoa/bookmark_bubble_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "grit/generated_resources.h"

@implementation BookmarkBubbleController

- (id)initWithDelegate:(id<BookmarkBubbleControllerDelegate>)delegate
          parentWindow:(NSWindow*)parentWindow
      topLeftForBubble:(NSPoint)topLeftForBubble
                 model:(BookmarkModel*)model
                  node:(const BookmarkNode*)node
     alreadyBookmarked:(BOOL)alreadyBookmarked {
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:@"BookmarkBubble"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
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

- (void)windowWillClose:(NSNotification *)notification {
  [self autorelease];
}

- (void)windowDidLoad {
  NSWindow* window = [self window];
  NSPoint origin = [parentWindow_ convertBaseToScreen:topLeftForBubble_];
  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];
  [parentWindow_ addChildWindow:window ordered:NSWindowAbove];
  // Default is IDS_BOOMARK_BUBBLE_PAGE_BOOKMARK; "Bookmark".
  // If adding for the 1st time the string becomes "Bookmark Added!"
  if (!alreadyBookmarked_) {
    NSString* title =
        l10n_util::GetNSString(IDS_BOOMARK_BUBBLE_PAGE_BOOKMARKED);
    [bigTitle_ setStringValue:title];
  }

  [self fillInFolderList];
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
  [super close];
}

// Shows the bookmark editor sheet for more advanced editing.
- (void)showEditor {
  [self updateBookmarkNode];
  [delegate_ editBookmarkNode:node_];
  [self close];
}

- (IBAction)edit:(id)sender {
  UserMetrics::RecordAction(L"BookmarkBubble_Edit", model_->profile());
  [self showEditor];
}

- (IBAction)ok:(id)sender {
  [self updateBookmarkNode];
  [self close];
}

// By implementing this, ESC causes the window to go away. If clicking the
// star was what prompted this bubble to appear (i.e., not already bookmarked),
// remove the bookmark.
- (IBAction)cancel:(id)sender {
  if (!alreadyBookmarked_) {
    // |-remove:| calls |-close| so we don't have to bother.
    [self remove:sender];
  } else {
    [self ok:sender];
  }
}

- (IBAction)remove:(id)sender {
  model_->SetURLStarred(node_->GetURL(), node_->GetTitle(), false);
  UserMetrics::RecordAction(L"BookmarkBubble_Unstar", model_->profile());
  node_ = NULL;  // no longer valid
  [self ok:sender];
}

// We are the delegate of the combo box so we can tell when "choose
// another folder" was picked.
- (void)comboBoxSelectionDidChange:(NSNotification*)notification {
  NSString* selected = [folderComboBox_ objectValueOfSelectedItem];
  if ([selected isEqual:chooseAnotherFolder_.get()]) {
    UserMetrics::RecordAction(L"BookmarkBubble_EditFromCombobox",
                              model_->profile());
    [self showEditor];
  }
}

// We are the delegate of our own window so we know when we lose key.
// When we lose key status we close, mirroring Windows behavior.
- (void)windowDidResignKey:(NSNotification*)notification {
  DCHECK_EQ([notification object], [self window]);

  // Can't call close from within a window delegate method. We can call
  // close after it's finished though. So this will call close for us next
  // time through the event loop.
  [self performSelector:@selector(ok:) withObject:self afterDelay:0];
}

@end  // BookmarkBubbleController


@implementation BookmarkBubbleController(ExposedForUnitTesting)


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
  if (!node_) return;

  // First the title...
  NSString* oldTitle = base::SysWideToNSString(node_->GetTitle());
  NSString* newTitle = [nameTextField_ stringValue];
  if (![oldTitle isEqual:newTitle]) {
    model_->SetTitle(node_, base::SysNSStringToWide(newTitle));
    UserMetrics::RecordAction(L"BookmarkBubble_ChangeTitleInBubble",
                              model_->profile());
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
      UserMetrics::RecordAction(L"BookmarkBubble_ChangeParent",
                                model_->profile());
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

- (NSComboBox*)folderComboBox {
  return folderComboBox_;
}

@end  // implementation BookmarkBubbleController(ExposedForUnitTesting)
