// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/tree_model.h"
#include "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/cookie_tree_node.h"
#include "chrome/browser/cookies_tree_model.h"
#include "net/base/cookie_monster.h"

@class CookiesWindowController;
class Profile;

namespace {
class CookiesWindowControllerTest;
}

// Thin bridge to the window controller that performs model update actions
// directly on the treeController_.
class CookiesTreeModelObserverBridge : public TreeModelObserver {
 public:
  explicit CookiesTreeModelObserverBridge(CookiesWindowController* controller);

  // Notification that nodes were added to the specified parent.
  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count);

  // Notification that nodes were removed from the specified parent.
  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count);

  // Notification the children of |parent| have been reordered. Note, only
  // the direct children of |parent| have been reordered, not descendants.
  virtual void TreeNodeChildrenReordered(TreeModel* model,
                                         TreeModelNode* parent);

  // Notification that the contents of a node has changed.
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node);

 private:
  friend class ::CookiesWindowControllerTest;

  // Creates an CocoaCookieTreeNodefrom a platform-independent one.
  // Return value is autoreleased. This can recusively create child nodes.
  CocoaCookieTreeNode* CocoaNodeFromTreeNode(TreeModelNode* node,
                                             bool recurse);

  // Finds the Cocoa model node based on a platform-independent one. This is
  // done by comparing the treeNode pointers. |start| is the node to start
  // searching at. If |start| is nil, the root is used.
  CocoaCookieTreeNode* FindCocoaNode(TreeModelNode* node,
                                     CocoaCookieTreeNode* start);

  CookiesWindowController* window_controller_;  // weak, owns us.
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

  Profile* profile_;  // weak
  BrowsingDataLocalStorageHelper* storageHelper_;  // weak
}
@property (assign, nonatomic) BOOL removeButtonEnabled;
@property (readonly, nonatomic) NSTreeController* treeController;

// Designated initializer. Profile cannot be NULL.
- (id)initWithProfile:(Profile*)profile
        storageHelper:(BrowsingDataLocalStorageHelper*)storageHelper;

// Shows the cookies window as a modal sheet attached to |window|.
- (void)attachSheetTo:(NSWindow*)window;

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
- (void)clearBrowsingDataNotification:(NSNotification*)notif;
- (CookiesTreeModelObserverBridge*)modelObserver;
- (NSArray*)icons;
- (void)loadTreeModelFromProfile;
@end
