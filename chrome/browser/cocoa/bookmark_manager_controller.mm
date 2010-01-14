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
#include "chrome/browser/bookmarks/bookmark_utils.h"
#import "chrome/browser/cocoa/bookmark_groups_controller.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#include "chrome/browser/profile.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"


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


@synthesize profile = profile_;

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
    bridge_.reset(new BookmarkManagerBridge(self));
    profile_->GetBookmarkModel()->AddObserver(bridge_.get());

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
  if (bridge_.get())
    profile_->GetBookmarkModel()->RemoveObserver(bridge_.get());
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
- (BookmarkItem*)itemFromNode:(const BookmarkNode*)node {
  if (!node)
    return nil;
  if (!nodeMap_) {
    nodeMap_.reset([[NSMapTable alloc]
        initWithKeyOptions:NSPointerFunctionsOpaqueMemory |
                           NSPointerFunctionsOpaquePersonality
              valueOptions:NSPointerFunctionsStrongMemory
                  capacity:500]);
  }
  BookmarkItem* item = (BookmarkItem*)NSMapGet(nodeMap_, node);
  if (!item) {
    item = [[BookmarkItem alloc] initWithBookmarkNode:node manager:self];
    NSMapInsertKnownAbsent(nodeMap_, node, item);
    [item release];
  }
  return item;
}

- (BookmarkItem*)bookmarkBarItem {
  return [self itemFromNode:[self bookmarkModel]->GetBookmarkBarNode()];
}

- (BookmarkItem*)otherBookmarksItem {
  return [self itemFromNode:[self bookmarkModel]->other_node()];
}

// Removes a BookmarkNode from the node<->item mapping table.
- (void)forgetNode:(const BookmarkNode*)node {
  NSMapRemove(nodeMap_, node);
  for (int i = node->GetChildCount() - 1 ; i >= 0 ; i--) {
    [self forgetNode:node->GetChild(i)];
  }
}

// Called when the bookmark model changes; forwards to the sub-controllers.
- (void)itemChanged:(BookmarkItem*)item
    childrenChanged:(BOOL)childrenChanged {
  if (item) {
    [groupsController_ itemChanged:item childrenChanged:childrenChanged];
    [treeController_ itemChanged:item childrenChanged:childrenChanged];
  }
}

// Called when the bookmark model changes; forwards to the sub-controllers.
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged {
  BookmarkItem* item = (BookmarkItem*)NSMapGet(nodeMap_, node);
  if (item) {
    [self itemChanged:item childrenChanged:childrenChanged];
  }
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

@end
