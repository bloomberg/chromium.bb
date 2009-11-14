// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "grit/generated_resources.h"

// An object to represent the ChooseAnotherFolder item in the pop up.
@interface ChooseAnotherFolder : NSObject
@end

@implementation ChooseAnotherFolder
@end

@interface BookmarkBubbleController ()
- (void)updateBookmarkNode;
- (void)fillInFolderList;
@end

@implementation BookmarkBubbleController

+ (id)chooseAnotherFolderObject {
  // Singleton object to act as a representedObject for the "choose another
  // folder" item in the pop up.
  static ChooseAnotherFolder* object = nil;
  if (!object) {
    object = [[ChooseAnotherFolder alloc] init];
  }
  return object;
}

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
  [self ok:nil];
  [delegate_ editBookmarkNode:node_];
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
    // |-remove:| calls |-close| so don't do it.
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

// The controller is  the target of the pop up button box action so it can
// handle when "choose another folder" was picked.
- (IBAction)folderChanged:(id)sender {
  DCHECK([sender isEqual:folderPopUpButton_]);
  NSMenuItem* selected = [folderPopUpButton_ selectedItem];
  ChooseAnotherFolder* chooseItem = [[self class] chooseAnotherFolderObject];
  if ([[selected representedObject] isEqual:chooseItem]) {
    UserMetrics::RecordAction(L"BookmarkBubble_EditFromCombobox",
                              model_->profile());
    [self showEditor];
  }
}

// The controller is the delegate of the window so it receives did resign key
// notifications. When key is resigned mirror Windows behavior and close the
// window.
- (void)windowDidResignKey:(NSNotification*)notification {
  DCHECK_EQ([notification object], [self window]);

  // Can't call close from within a window delegate method. Call close for the
  // next time through the event loop.
  [self performSelector:@selector(ok:) withObject:self afterDelay:0];
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
  const BookmarkNode* oldParent = node_->GetParent();
  NSMenuItem* selectedItem = [folderPopUpButton_ selectedItem];
  id representedObject = [selectedItem representedObject];
  if ([representedObject isEqual:[[self class] chooseAnotherFolderObject]]) {
    // "Choose another folder..."
    return;
  }
  const BookmarkNode* newParent =
      static_cast<const BookmarkNode*>([representedObject pointerValue]);
  DCHECK(newParent);
  if (oldParent != newParent) {
    int index = newParent->GetChildCount();
    model_->Move(node_, newParent, index);
    UserMetrics::RecordAction(L"BookmarkBubble_ChangeParent",
                              model_->profile());
  }
}

// Fill in all information related to the folder pop up button.
- (void)fillInFolderList {
  [nameTextField_ setStringValue:base::SysWideToNSString(node_->GetTitle())];
  DCHECK([folderPopUpButton_ numberOfItems] == 0);
  [self addFolderNodes:model_->root_node() toPopUpButton:folderPopUpButton_];
  NSMenu* menu = [folderPopUpButton_ menu];
  NSString* title = [[self class] chooseAnotherFolderString];
  NSMenuItem *item = [menu addItemWithTitle:title
                                     action:NULL
                              keyEquivalent:@""];
  ChooseAnotherFolder* obj = [[self class] chooseAnotherFolderObject];
  [item setRepresentedObject:obj];
  // Finally, select the current parent.
  NSValue* parentValue = [NSValue valueWithPointer:node_->GetParent()];
  NSInteger idx = [menu indexOfItemWithRepresentedObject:parentValue];
  [folderPopUpButton_ selectItemAtIndex:idx];
}

@end  // BookmarkBubbleController


@implementation BookmarkBubbleController(ExposedForUnitTesting)

+ (NSString*)chooseAnotherFolderString {
  return l10n_util::GetNSStringWithFixup(
      IDS_BOOMARK_BUBBLE_CHOOSER_ANOTHER_FOLDER);
}

// For the given folder node, walk the tree and add folder names to
// the given pop up button.
- (void)addFolderNodes:(const BookmarkNode*)parent
         toPopUpButton:(NSPopUpButton*)button {
  NSString* title = base::SysWideToNSString(parent->GetTitle());
  if ([title length])  {  // no title if root
    NSMenu* menu = [button menu];
    NSMenuItem* item = [menu addItemWithTitle:title
                                       action:NULL
                                keyEquivalent:@""];
    [item setRepresentedObject:[NSValue valueWithPointer:parent]];
  }
  for (int i = 0; i < parent->GetChildCount(); i++) {
    const BookmarkNode* child = parent->GetChild(i);
    if (child->is_folder())
      [self addFolderNodes:child toPopUpButton:button];
  }
}

- (void)setTitle:(NSString*)title parentFolder:(const BookmarkNode*)parent {
  [nameTextField_ setStringValue:title];
  [self setParentFolderSelection:parent];
}

// Pick a specific parent node in the selection by finding the right
// pop up button index.
- (void)setParentFolderSelection:(const BookmarkNode*)parent {
  // Expectation: There is a parent mapping for all items in the
  // folderPopUpButton except the last one ("Choose another folder...").
  NSMenu* menu = [folderPopUpButton_ menu];
  NSValue* parentValue = [NSValue valueWithPointer:parent];
  NSInteger idx = [menu indexOfItemWithRepresentedObject:parentValue];
  DCHECK(idx != -1);
  [folderPopUpButton_ selectItemAtIndex:idx];
}

- (NSPopUpButton*)folderPopUpButton {
  return folderPopUpButton_;
}

@end  // implementation BookmarkBubbleController(ExposedForUnitTesting)
