// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_manager_controller.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/bookmark_groups_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#include "chrome/browser/profile.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"


// There's at most one BookmarkManagerController at a time. This points to it.
static BookmarkManagerController* sInstance;


@interface BookmarkManagerController ()
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged;
@end


// Adapter to tell BookmarkManagerController when bookmarks change.
class BookmarkManagerBridge : public BookmarkModelObserver {
 public:
  BookmarkManagerBridge(BookmarkManagerController* manager)
      :manager_(manager) { }

  virtual void Loaded(BookmarkModel* model) {
    // Ignore this; model has already loaded by this point.
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {
    [manager_ nodeChanged:old_parent childrenChanged:YES];
    [manager_ nodeChanged:new_parent childrenChanged:YES];
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {
    [manager_ nodeChanged:parent childrenChanged:YES];
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {
    [manager_ nodeChanged:parent childrenChanged:YES];
    [manager_ forgetNode:node];
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {
    [manager_ nodeChanged:node childrenChanged:NO];
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
    [manager_ nodeChanged:node childrenChanged:NO];
  }

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {
    [manager_ nodeChanged:node childrenChanged:YES];
  }

 private:
  BookmarkManagerController* manager_;  // weak
};


@implementation BookmarkManagerController


// Private instance initialization method.
- (id)initWithProfile:(Profile*)profile {
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibPath = [mac_util::MainAppBundle()
                                            pathForResource:@"BookmarkManager"
                                                     ofType:@"nib"];
  self = [super initWithWindowNibPath:nibPath owner:self];
  if (self != nil) {
    profile_ = profile;
    bridge_ = new BookmarkManagerBridge(self);
    profile_->GetBookmarkModel()->AddObserver(bridge_);

    // Initialize some cached icons:
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folderIcon_.reset([rb.GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER) retain]);
    defaultFavIcon_.reset([rb.GetNSImageNamed(IDR_DEFAULT_FAVICON) retain]);
  }
  return self;
}

- (void)dealloc {
  if (self == sInstance) {
    sInstance = nil;
  }
  if (bridge_) {
    profile_->GetBookmarkModel()->RemoveObserver(bridge_);
    delete bridge_;
  }
  [super dealloc];
}

- (void)awakeFromNib {
  [groupsController_ reload];
  [treeController_ bind:@"group"
               toObject:groupsController_
            withKeyPath:@"selectedGroup"
                options:0];
}

// can't synthesize category methods, unfortunately
- (BookmarkGroupsController*)groupsController {
  return groupsController_;
}

- (BookmarkTreeController*)treeController {
  return treeController_;
}

static void addItem(NSMenu* menu, int command, SEL action) {
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(command)
                  action:action
           keyEquivalent:@""];
}

// Generates a context menu for use by the group/tree controllers.
- (NSMenu*)contextMenu {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  addItem(menu, IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB, @selector(openItems:));
  [menu addItem:[NSMenuItem separatorItem]];
  addItem(menu, IDS_BOOKMARK_BAR_EDIT, @selector(editTitle:));
  addItem(menu, IDS_BOOKMARK_BAR_REMOVE, @selector(delete:));
  [menu addItem:[NSMenuItem separatorItem]];
  addItem(menu, IDS_BOOMARK_BAR_NEW_FOLDER, @selector(newFolder:));
  return menu;
}


#pragma mark -
#pragma mark DATA MODEL:


// Getter for the |bookmarkModel| property.
- (BookmarkModel*)bookmarkModel {
  return profile_->GetBookmarkModel();
}

// Maps a BookmarkNode to a table/outline row item placeholder.
- (const BookmarkNode*)nodeFromItem:(id)item {
  return (const BookmarkNode*)[item pointerValue];
}

// Maps a table/outline row item placeholder back to a BookmarkNode.
- (id)itemFromNode:(const BookmarkNode*)node {
  if (!nodeMap_) {
    nodeMap_.reset([[NSMapTable alloc]
        initWithKeyOptions:NSPointerFunctionsOpaqueMemory |
                           NSPointerFunctionsOpaquePersonality
              valueOptions:NSPointerFunctionsStrongMemory
                  capacity:500]);
  }
  NSValue* item = (NSValue*)NSMapGet(nodeMap_, node);
  if (!item) {
    item = [NSValue valueWithPointer:node];
    NSMapInsertKnownAbsent(nodeMap_, node, item);
  }
  return item;
}

// Removes a BookmarkNode from the node<->item mapping table.
- (void)forgetNode:(const BookmarkNode*)node {
  NSMapRemove(nodeMap_, node);
  for (int i = node->GetChildCount() - 1 ; i >= 0 ; i--) {
    [self forgetNode:node->GetChild(i)];
  }
}

// Called when the bookmark model changes; forwards to the sub-controllers.
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged {
  [groupsController_ nodeChanged:node childrenChanged:childrenChanged];
  // TreeController only cares about nodes we have items for, so don't bother
  // creating a new item if the node's never been seen:
  id item = (NSValue*)NSMapGet(nodeMap_, node);
  if (item) {
    [treeController_ itemChanged:item childrenChanged:childrenChanged];
  }
}


// Returns the icon (fav- or folder) for a table/outline item.
- (NSImage*)iconForItem:(id)item {
    const BookmarkNode* node = [self nodeFromItem:item];
  if (node->is_folder()) {
    return folderIcon_;
  } else if (node->is_url()) {
    const BookmarkNode* node = [self nodeFromItem:item];
    const SkBitmap& skIcon = [self bookmarkModel]->GetFavIcon(node);
    if (!skIcon.isNull()) {
      return gfx::SkBitmapToNSImage(skIcon);
    }
  }
  return defaultFavIcon_;
}


#pragma mark -
#pragma mark WINDOW MANAGEMENT:


// Public entry point to open the bookmark manager.
+ (BookmarkManagerController*)showBookmarkManager:(Profile*)profile
{
  if (!sInstance) {
    sInstance = [[self alloc] initWithProfile:profile];
  }
  [sInstance showWindow:self];
  return sInstance;
}

// When window closes, get rid of myself too. (NSWindow delegate)
- (void)windowWillClose:(NSNotification*)n {
  [self autorelease];
}

// Install the search field into the search toolbar item. (NSToolbar delegate)
- (void)toolbarWillAddItem:(NSNotification*)notification {
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ([[item itemIdentifier] isEqualToString:@"search"]) {
    [item setView:toolbarSearchView_];
    NSSize size = [toolbarSearchView_ frame].size;
    [item setMinSize:size];
    [item setMaxSize:size];
  }
}

// Called when the user types into the search field.
- (IBAction)searchFieldChanged:(id)sender {
  //TODO(snej): Implement this
}


// Open a bookmark, by having Chrome open a tab on its URL.
- (void)openBookmarkItem:(id)item {
  const BookmarkNode* node = [self nodeFromItem:item];
  DCHECK(node);
  if (!node->is_url())
    return;
  GURL url = node->GetURL();

  Browser* browser = BrowserList::GetLastActive();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create(profile_);
    browser->window()->Show();
  }
  browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::AUTO_BOOKMARK);
}

@end
