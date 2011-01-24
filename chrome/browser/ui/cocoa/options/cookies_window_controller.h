// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cookies_tree_model.h"
#import "chrome/browser/ui/cocoa/content_settings/cookie_tree_node.h"
#include "net/base/cookie_monster.h"

@class CookiesWindowController;
@class CookieDetailsViewController;
class Profile;

namespace {
class CookiesWindowControllerTest;
}

namespace ui {
class TreeModel;
class TreeModelNode;
}

// Thin bridge to the window controller that performs model update actions
// directly on the treeController_.
class CookiesTreeModelObserverBridge : public CookiesTreeModel::Observer {
 public:
  explicit CookiesTreeModelObserverBridge(CookiesWindowController* controller);

  // Begin TreeModelObserver implementation.
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count);
  virtual void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node);
  // End TreeModelObserver implementation.

  virtual void TreeModelBeginBatch(CookiesTreeModel* model);
  virtual void TreeModelEndBatch(CookiesTreeModel* model);

  // Invalidates the Cocoa model. This is used to tear down the Cocoa model
  // when we're about to entirely rebuild it.
  void InvalidateCocoaModel();

 private:
  friend class ::CookiesWindowControllerTest;

  // Creates a CocoaCookieTreeNode from a platform-independent one.
  // Return value is autoreleased. This creates child nodes recusively.
  CocoaCookieTreeNode* CocoaNodeFromTreeNode(ui::TreeModelNode* node);

  // Finds the Cocoa model node based on a platform-independent one. This is
  // done by comparing the treeNode pointers. |start| is the node to start
  // searching at. If |start| is nil, the root is used.
  CocoaCookieTreeNode* FindCocoaNode(ui::TreeModelNode* node,
                                     CocoaCookieTreeNode* start);

  // Returns whether or not the Cocoa tree model is built.
  bool HasCocoaModel();

  CookiesWindowController* window_controller_;  // weak, owns us.

  // If this is true, then the Model has informed us that it is batching
  // updates. Rather than updating the Cocoa side of the model, we ignore those
  // small changes and rebuild once at the end.
  bool batch_update_;
};

// Controller for the cookies manager. This class stores an internal copy of
// the CookiesTreeModel but with Cocoa-converted values (NSStrings and NSImages
// instead of std::strings and SkBitmaps). Doing this allows us to use bindings
// for the interface. Changes are pushed to this internal model via a very thin
// bridge (see above).
@interface CookiesWindowController : NSWindowController
                                     <NSOutlineViewDelegate,
                                      NSWindowDelegate> {
 @private
  // Platform-independent model and C++/Obj-C bridge components.
  scoped_ptr<CookiesTreeModel> treeModel_;
  scoped_ptr<CookiesTreeModelObserverBridge> modelObserver_;

  // Cached array of icons.
  scoped_nsobject<NSMutableArray> icons_;

  // Our Cocoa copy of the model.
  scoped_nsobject<CocoaCookieTreeNode> cocoaTreeModel_;

  // A flag indicating whether or not the "Remove" button should be enabled.
  BOOL removeButtonEnabled_;

  IBOutlet NSTreeController* treeController_;
  IBOutlet NSOutlineView* outlineView_;
  IBOutlet NSSearchField* searchField_;
  IBOutlet NSView* cookieDetailsViewPlaceholder_;
  IBOutlet NSButton* removeButton_;

  scoped_nsobject<CookieDetailsViewController> detailsViewController_;
  Profile* profile_;  // weak
  BrowsingDataDatabaseHelper* databaseHelper_;  // weak
  BrowsingDataLocalStorageHelper* storageHelper_;  // weak
  BrowsingDataAppCacheHelper* appcacheHelper_;  // weak
  BrowsingDataIndexedDBHelper* indexedDBHelper_;  // weak
}
@property(assign, nonatomic) BOOL removeButtonEnabled;
@property(readonly, nonatomic) NSTreeController* treeController;

// Designated initializer. Profile cannot be NULL.
- (id)initWithProfile:(Profile*)profile
       databaseHelper:(BrowsingDataDatabaseHelper*)databaseHelper
        storageHelper:(BrowsingDataLocalStorageHelper*)storageHelper
       appcacheHelper:(BrowsingDataAppCacheHelper*)appcacheHelper
      indexedDBHelper:(BrowsingDataIndexedDBHelper*)indexedDBHelper;

// Shows the cookies window as a modal sheet attached to |window|.
- (void)attachSheetTo:(NSWindow*)window;

// Updates the filter from the search field.
- (IBAction)updateFilter:(id)sender;

// Delete cookie actions.
- (IBAction)deleteCookie:(id)sender;
- (IBAction)deleteAllCookies:(id)sender;

// Closes the sheet and ends the modal loop. This will also cleanup the memory.
- (IBAction)closeSheet:(id)sender;

// Returns the cocoaTreeModel_.
- (CocoaCookieTreeNode*)cocoaTreeModel;
- (void)setCocoaTreeModel:(CocoaCookieTreeNode*)model;

// Returns the treeModel_.
- (CookiesTreeModel*)treeModel;

@end

@interface CookiesWindowController (UnitTesting)
- (void)deleteNodeAtIndexPath:(NSIndexPath*)path;
- (void)clearBrowsingDataNotification:(NSNotification*)notif;
- (CookiesTreeModelObserverBridge*)modelObserver;
- (NSArray*)icons;
- (void)loadTreeModelFromProfile;
@end
