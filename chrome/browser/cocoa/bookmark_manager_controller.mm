// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_manager_controller.h"

#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"


// Max number of recently-added bookmarks to show.
static const int kMaxRecents = 200;

// There's at most one BookmarkManagerController at a time. This points to it.
static BookmarkManagerController* sInstance;


@interface BookmarkManagerController ()
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged;
- (void)updateRecents;
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

    // Initialize the Recents and Search Results groups:
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    NSImage* recentIcon = rb.GetNSImageNamed(IDR_BOOKMARK_MANAGER_RECENT_ICON);
    recentGroup_.reset([[FakeBookmarkItem alloc] initWithTitle:@"Recently Added"
                                                          icon:recentIcon
                                                       manager:self]);
    NSImage* searchIcon = rb.GetNSImageNamed(IDR_BOOKMARK_MANAGER_SEARCH_ICON);
    searchGroup_.reset([[FakeBookmarkItem alloc] initWithTitle:@"Search Results"
                                                          icon:searchIcon
                                                       manager:self]);
  }
  return self;
}

- (void)dealloc {
  if (self == sInstance) {
    sInstance = nil;
  }
  [groupsController_ removeObserver:self forKeyPath:@"selectedItem"];
  if (bridge_.get())
    profile_->GetBookmarkModel()->RemoveObserver(bridge_.get());
  [super dealloc];
}

- (void)awakeFromNib {
  // Synthesize the hierarchy of the left-hand outline view.
  BookmarkModel* model = [self bookmarkModel];
  BookmarkItem* bar = [self itemFromNode:model->GetBookmarkBarNode()];
  BookmarkItem* other = [self itemFromNode:model->other_node()];
  NSArray* rootItems = [NSArray arrayWithObjects:
      bar,
      other,
      recentGroup_.get(),
      nil];
  FakeBookmarkItem* root = [[FakeBookmarkItem alloc] initWithTitle:@""
                                                              icon:nil
                                                           manager:self];
  [root setChildren:rootItems];
  [recentGroup_ setParent:root];
  [searchGroup_ setParent:root];
  [groupsController_ setGroup:root];

  // Turning on autosave also loads and applies the settings, which we couldn't
  // do until setting up the data model, above.
  NSOutlineView* outline = [groupsController_ outline];
  [outline setAutosaveExpandedItems:YES];
  if (![outline isItemExpanded:bar] && ![outline isItemExpanded:other]) {
    // By default, expand the Bookmarks Bar and Other:
    [groupsController_ expandItem:bar];
    [groupsController_ expandItem:other];
  }

  // The Source-List style on the group outline has less space between rows,
  // so compensate for this by increasing the spacing:
  NSSize spacing = [[groupsController_ outline] intercellSpacing];
  spacing.height += 2;
  [[groupsController_ outline] setIntercellSpacing:spacing];

  [listController_ setShowsLeaves:YES];
  [listController_ setFlat:YES];

  // Observe selection changes in the groups outline.
  [groupsController_ addObserver:self
                      forKeyPath:@"selectedItem"
                         options:NSKeyValueObservingOptionInitial
                         context:NULL];
}

// Install the search field into the search toolbar item. (NSToolbar delegate)
- (void)toolbarWillAddItem:(NSNotification*)notification {
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ([item tag] == 1) {
    DCHECK(toolbarSearchView_);
    [item setView:toolbarSearchView_];
    NSSize size = [toolbarSearchView_ frame].size;
    [item setMinSize:size];
    [item setMaxSize:size];
  }
}

// When window closes, get rid of myself too. (NSWindow delegate)
- (void)windowWillClose:(NSNotification*)n {
  [self autorelease];
}


#pragma mark -
#pragma mark ACCESSORS:


// can't synthesize category methods, unfortunately
- (BookmarkTreeController*)groupsController {
  return groupsController_;
}

- (BookmarkTreeController*)listController {
  return listController_;
}

// Returns the groups or list controller, whichever one has focus.
- (BookmarkTreeController*)focusedController {
  id first = [[self window] firstResponder];
  if ([first isKindOfClass:[BookmarksOutlineView class]])
    return [(BookmarksOutlineView*)first bookmarkController];
  return nil;
}

- (FakeBookmarkItem*)recentGroup {
  return recentGroup_;
}

- (FakeBookmarkItem*)searchGroup {
  return searchGroup_;
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
    [item nodeChanged];
    [groupsController_ itemChanged:item childrenChanged:childrenChanged];
    [listController_ itemChanged:item childrenChanged:childrenChanged];
  }

  // Update the recents or search results if they're visible.
  if ([groupsController_ selectedItem] == searchGroup_.get())
    [self searchFieldChanged:self];
  if ([groupsController_ selectedItem] == recentGroup_.get())
    [self updateRecents];
}

// Called when the bookmark model changes; forwards to the sub-controllers.
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged {
  BookmarkItem* item = (BookmarkItem*)NSMapGet(nodeMap_, node);
  if (item) {
    [self itemChanged:item childrenChanged:childrenChanged];
  }
}

- (void)updateRecents {
  typedef std::vector<const BookmarkNode*> NodeVector;
  NodeVector nodes;
  bookmark_utils::GetMostRecentlyAddedEntries(
      [self bookmarkModel], kMaxRecents, &nodes);

  // Update recentGroup_:
  NSMutableArray* result = [NSMutableArray arrayWithCapacity:nodes.size()];
  for (NodeVector::iterator it = nodes.begin(); it != nodes.end(); ++it) {
    [result addObject:[self itemFromNode:*it]];
  }
  if (![result isEqual:[recentGroup_ children]]) {
    [recentGroup_ setChildren:result];
    [self itemChanged:recentGroup_ childrenChanged:YES];
  }
}

- (void)updateSearch {
  typedef std::vector<const BookmarkNode*> MatchVector;
  MatchVector matches;
  NSString* searchString = [toolbarSearchView_ stringValue];
  if ([searchString length] > 0) {
    // Search in the BookmarkModel:
    std::wstring text = base::SysNSStringToWide(searchString);
    bookmark_utils::GetBookmarksContainingText(
        [self bookmarkModel],
        base::SysNSStringToWide(searchString),
        std::numeric_limits<int>::max(),  // unlimited result count
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
        &matches);
  }

  // Update contents of searchGroup_:
  NSMutableArray* result = [NSMutableArray arrayWithCapacity:matches.size()];
  for (MatchVector::iterator it = matches.begin(); it != matches.end(); ++it) {
    [result addObject:[self itemFromNode:*it]];
  }
  if (![result isEqual:[searchGroup_ children]]) {
    [searchGroup_ setChildren:result];
    [self itemChanged:searchGroup_ childrenChanged:YES];
  }

  // Show searchGroup_ if it's not visible yet:
  FakeBookmarkItem* root = (FakeBookmarkItem*)[groupsController_ group];
  NSArray* rootItems = [root children];
  if (![rootItems containsObject:searchGroup_]) {
    [root setChildren:[rootItems arrayByAddingObject:searchGroup_]];
    [self itemChanged:root childrenChanged:YES];
  }
}

- (void)selectedGroupChanged {
  BOOL showFolders = YES;
  BookmarkItem* group = [groupsController_ selectedItem];
  if (group == recentGroup_.get())
    [self updateRecents];
  else if (group == searchGroup_.get())
    [self updateSearch];
  else
    showFolders = NO;
  [listController_ setGroup:group];
  [listController_ setShowsFolderColumn:showFolders];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (object == groupsController_)
    [self selectedGroupChanged];
}

#pragma mark -
#pragma mark ACTIONS:


// Public entry point to open the bookmark manager.
+ (BookmarkManagerController*)showBookmarkManager:(Profile*)profile
{
  if (!sInstance) {
    sInstance = [[self alloc] initWithProfile:profile];
  }
  [sInstance showWindow:self];
  return sInstance;
}

- (void)showGroup:(BookmarkItem*)group {
  [groupsController_ revealItem:group];
}

// Makes an item visible and selects it.
- (BOOL)revealItem:(BookmarkItem*)item {
  return [groupsController_ revealItem:[item parent]] &&
      [listController_ revealItem:item];
}

// Called when the user types into the search field.
- (IBAction)searchFieldChanged:(id)sender {
  [self updateSearch];
  if ([[toolbarSearchView_ stringValue] length])
    [self showGroup:searchGroup_];
}

- (void)setSearchString:(NSString*)string {
  [toolbarSearchView_ setStringValue:string];
  [self searchFieldChanged:self];
}

- (IBAction)newFolder:(id)sender {
  [[self focusedController] newFolder:sender];
}

- (IBAction)delete:(id)sender {
  [[self focusedController] delete:sender];
}

// Called when the user picks a menu or toolbar item when this window is key.
- (void)commandDispatch:(id)sender {
  // Copied from app_controller_mac.mm:
  // Handle the case where we're dispatching a command from a sender that's in a
  // browser window. This means that the command came from a background window
  // and is getting here because the foreground window is not a browser window.
  if ([sender respondsToSelector:@selector(window)]) {
    id delegate = [[sender window] windowController];
    if ([delegate isKindOfClass:[BrowserWindowController class]]) {
      [delegate commandDispatch:sender];
      return;
    }
  }

  if ([sender tag] == IDC_FIND) {
    [[[self window] toolbar] setVisible:YES];
    [[self window] makeFirstResponder:toolbarSearchView_];
  }
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandDispatchUsingKeyModifiers:)) {
    NSInteger tag = [item tag];
    return (tag == IDC_FIND);
  } else if (action == @selector(newFolder:) || action == @selector(delete:)) {
    return [[self focusedController] validateUserInterfaceItem:item];
  }
  return YES;
}

@end
