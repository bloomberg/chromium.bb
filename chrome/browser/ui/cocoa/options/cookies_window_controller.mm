// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/cookies_window_controller.h"

#include <queue>
#include <vector>

#include "app/l10n_util_mac.h"
#import "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/clear_browsing_data_controller.h"
#include "chrome/browser/ui/cocoa/content_settings/cookie_details_view_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/apple/ImageAndTextCell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

// Key path used for notifying KVO.
static NSString* const kCocoaTreeModel = @"cocoaTreeModel";

CookiesTreeModelObserverBridge::CookiesTreeModelObserverBridge(
    CookiesWindowController* controller)
    : window_controller_(controller),
      batch_update_(false) {
}

// Notification that nodes were added to the specified parent.
void CookiesTreeModelObserverBridge::TreeNodesAdded(ui::TreeModel* model,
                                                    ui::TreeModelNode* parent,
                                                    int start,
                                                    int count) {
  // We're in for a major rebuild. Ignore this request.
  if (batch_update_ || !HasCocoaModel())
    return;

  CocoaCookieTreeNode* cocoa_parent = FindCocoaNode(parent, nil);
  NSMutableArray* cocoa_children = [cocoa_parent mutableChildren];

  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  CookieTreeNode* cookie_parent = static_cast<CookieTreeNode*>(parent);
  for (int i = 0; i < count; ++i) {
    CookieTreeNode* cookie_child = cookie_parent->GetChild(start + i);
    CocoaCookieTreeNode* new_child = CocoaNodeFromTreeNode(cookie_child);
    [cocoa_children addObject:new_child];
  }
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

// Notification that nodes were removed from the specified parent.
void CookiesTreeModelObserverBridge::TreeNodesRemoved(ui::TreeModel* model,
                                                      ui::TreeModelNode* parent,
                                                      int start,
                                                      int count) {
  // We're in for a major rebuild. Ignore this request.
  if (batch_update_ || !HasCocoaModel())
    return;

  CocoaCookieTreeNode* cocoa_parent = FindCocoaNode(parent, nil);
  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  NSMutableArray* cocoa_children = [cocoa_parent mutableChildren];
  for (int i = start + count - 1; i >= start; --i) {
    [cocoa_children removeObjectAtIndex:i];
  }
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

// Notification that the contents of a node has changed.
void CookiesTreeModelObserverBridge::TreeNodeChanged(ui::TreeModel* model,
                                                     ui::TreeModelNode* node) {
  // If we don't have a Cocoa model, only let the root node change.
  if (batch_update_ || (!HasCocoaModel() && model->GetRoot() != node))
    return;

  if (HasCocoaModel()) {
    // We still have a Cocoa model, so just rebuild the node.
    [window_controller_ willChangeValueForKey:kCocoaTreeModel];
    CocoaCookieTreeNode* changed_node = FindCocoaNode(node, nil);
    [changed_node rebuild];
    [window_controller_ didChangeValueForKey:kCocoaTreeModel];
  } else {
    // Full rebuild.
    [window_controller_ setCocoaTreeModel:CocoaNodeFromTreeNode(node)];
  }
}

void CookiesTreeModelObserverBridge::TreeModelBeginBatch(
    CookiesTreeModel* model) {
  batch_update_ = true;
}

void CookiesTreeModelObserverBridge::TreeModelEndBatch(
    CookiesTreeModel* model) {
  DCHECK(batch_update_);
  CocoaCookieTreeNode* root = CocoaNodeFromTreeNode(model->GetRoot());
  [window_controller_ setCocoaTreeModel:root];
  batch_update_ = false;
}

void CookiesTreeModelObserverBridge::InvalidateCocoaModel() {
  [[[window_controller_ cocoaTreeModel] mutableChildren] removeAllObjects];
}

CocoaCookieTreeNode* CookiesTreeModelObserverBridge::CocoaNodeFromTreeNode(
    ui::TreeModelNode* node) {
  CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
  return [[[CocoaCookieTreeNode alloc] initWithNode:cookie_node] autorelease];
}

// Does breadth-first search on the tree to find |node|. This method is most
// commonly used to find origin/folder nodes, which are at the first level off
// the root (hence breadth-first search).
CocoaCookieTreeNode* CookiesTreeModelObserverBridge::FindCocoaNode(
    ui::TreeModelNode* target, CocoaCookieTreeNode* start) {
  if (!start) {
    start = [window_controller_ cocoaTreeModel];
  }
  if ([start treeNode] == target) {
    return start;
  }

  // Enqueue the root node of the search (sub-)tree.
  std::queue<CocoaCookieTreeNode*> horizon;
  horizon.push(start);

  // Loop until we've looked at every node or we found the target.
  while (!horizon.empty()) {
    // Dequeue the item at the front.
    CocoaCookieTreeNode* node = horizon.front();
    horizon.pop();

    // If this is the droid we're looking for, report it.
    if ([node treeNode] == target)
      return node;

    // "Move along, move along." by adding all child nodes to the queue.
    if (![node isLeaf]) {
      NSArray* children = [node children];
      for (CocoaCookieTreeNode* child in children) {
        horizon.push(child);
      }
    }
  }

  return nil;  // We couldn't find the node.
}

// Returns whether or not the Cocoa tree model is built.
bool CookiesTreeModelObserverBridge::HasCocoaModel() {
  return ([[[window_controller_ cocoaTreeModel] children] count] > 0U);
}

#pragma mark Window Controller

@implementation CookiesWindowController

@synthesize removeButtonEnabled = removeButtonEnabled_;
@synthesize treeController = treeController_;

- (id)initWithProfile:(Profile*)profile
       databaseHelper:(BrowsingDataDatabaseHelper*)databaseHelper
        storageHelper:(BrowsingDataLocalStorageHelper*)storageHelper
       appcacheHelper:(BrowsingDataAppCacheHelper*)appcacheHelper
      indexedDBHelper:(BrowsingDataIndexedDBHelper*)indexedDBHelper {
  DCHECK(profile);
  NSString* nibpath = [base::mac::MainAppBundle() pathForResource:@"Cookies"
                                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    databaseHelper_ = databaseHelper;
    storageHelper_ = storageHelper;
    appcacheHelper_ = appcacheHelper;
    indexedDBHelper_ = indexedDBHelper;

    [self loadTreeModelFromProfile];

    // Register for Clear Browsing Data controller so we update appropriately.
    ClearBrowsingDataController* clearingController =
        [ClearBrowsingDataController controllerForProfile:profile_];
    if (clearingController) {
      NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
      [center addObserver:self
                 selector:@selector(clearBrowsingDataNotification:)
                     name:kClearBrowsingDataControllerDidDelete
                   object:clearingController];
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  detailsViewController_.reset([[CookieDetailsViewController alloc] init]);

  NSView* detailView = [detailsViewController_.get() view];
  NSRect viewFrameRect = [cookieDetailsViewPlaceholder_ frame];
  [[detailsViewController_.get() view] setFrame:viewFrameRect];
  [[cookieDetailsViewPlaceholder_ superview]
      replaceSubview:cookieDetailsViewPlaceholder_
                with:detailView];

  [detailsViewController_ configureBindingsForTreeController:treeController_];
}

- (void)windowWillClose:(NSNotification*)notif {
  [searchField_ setTarget:nil];
  [outlineView_ setDelegate:nil];
  [self autorelease];
}

- (void)attachSheetTo:(NSWindow*)window {
  [NSApp beginSheet:[self window]
     modalForWindow:window
      modalDelegate:self
     didEndSelector:@selector(sheetEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)sheetEndSheet:(NSWindow*)sheet
          returnCode:(NSInteger)returnCode
         contextInfo:(void*)context {
  [sheet close];
  [sheet orderOut:self];
}

- (IBAction)updateFilter:(id)sender {
  DCHECK([sender isKindOfClass:[NSSearchField class]]);
  NSString* string = [sender stringValue];
  // Invalidate the model here because all the nodes are going to be removed
  // in UpdateSearchResults(). This could lead to there temporarily being
  // invalid pointers in the Cocoa model.
  modelObserver_->InvalidateCocoaModel();
  treeModel_->UpdateSearchResults(base::SysNSStringToWide(string));
}

- (IBAction)deleteCookie:(id)sender {
  DCHECK_EQ(1U, [[treeController_ selectedObjects] count]);
  [self deleteNodeAtIndexPath:[treeController_ selectionIndexPath]];
}

// This will delete the Cocoa model node as well as the backing model object at
// the specified index path in the Cocoa model. If the node that was deleted
// was the sole child of the parent node, this will be called recursively to
// delete empty parents.
- (void)deleteNodeAtIndexPath:(NSIndexPath*)path {
  NSTreeNode* treeNode =
      [[treeController_ arrangedObjects] descendantNodeAtIndexPath:path];
  if (!treeNode)
    return;

  CocoaCookieTreeNode* node = [treeNode representedObject];
  CookieTreeNode* cookie = static_cast<CookieTreeNode*>([node treeNode]);
  treeModel_->DeleteCookieNode(cookie);
  // If there is a next cookie, this will select it because items will slide
  // up. If there is no next cookie, this is a no-op.
  [treeController_ setSelectionIndexPath:path];
  // If the above setting of the selection was in fact a no-op, find the next
  // node to select.
  if (![[treeController_ selectedObjects] count]) {
    NSUInteger lastIndex = [path indexAtPosition:[path length] - 1];
    if (lastIndex != 0) {
      // If there any nodes remaining, select the node that is in the list
      // before this one.
      path = [path indexPathByRemovingLastIndex];
      path = [path indexPathByAddingIndex:lastIndex - 1];
      [treeController_ setSelectionIndexPath:path];
    }
  }
}

- (IBAction)deleteAllCookies:(id)sender {
  // Preemptively delete all cookies in the Cocoa model.
  modelObserver_->InvalidateCocoaModel();
  treeModel_->DeleteAllStoredObjects();
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)clearBrowsingDataNotification:(NSNotification*)notif {
  NSNumber* removeMask =
      [[notif userInfo] objectForKey:kClearBrowsingDataControllerRemoveMask];
  if ([removeMask intValue] & BrowsingDataRemover::REMOVE_COOKIES) {
    [self loadTreeModelFromProfile];
  }
}

// Override keyDown on the controller (which is the first responder) to allow
// both backspace and delete to be captured by the Remove button.
- (void)keyDown:(NSEvent*)theEvent {
  NSString* keys = [theEvent characters];
  if ([keys length]) {
    unichar key = [keys characterAtIndex:0];
    // The button has a key equivalent of backspace, so examine this event for
    // forward delete.
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        [self removeButtonEnabled]) {
      [removeButton_ performClick:self];
      return;
    }
  }
  [super keyDown:theEvent];
}

#pragma mark Getters and Setters

- (CocoaCookieTreeNode*)cocoaTreeModel {
  return cocoaTreeModel_.get();
}
- (void)setCocoaTreeModel:(CocoaCookieTreeNode*)model {
  cocoaTreeModel_.reset([model retain]);
}

- (CookiesTreeModel*)treeModel {
  return treeModel_.get();
}

#pragma mark Outline View Delegate

- (void)outlineView:(NSOutlineView*)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn*)tableColumn
               item:(id)item {
  CocoaCookieTreeNode* node = [item representedObject];
  int index = treeModel_->GetIconIndex([node treeNode]);
  NSImage* icon = nil;
  if (index >= 0)
    icon = [icons_ objectAtIndex:index];
  else
    icon = [icons_ lastObject];
  [(ImageAndTextCell*)cell setImage:icon];
}

- (void)outlineViewItemDidExpand:(NSNotification*)notif {
  NSTreeNode* item = [[notif userInfo] objectForKey:@"NSObject"];
  CocoaCookieTreeNode* node = [item representedObject];
  NSArray* children = [node children];
  if ([children count] == 1U) {
    // The node that will expand has one child. Do the user a favor and expand
    // that node (saving her a click) if it is non-leaf.
    CocoaCookieTreeNode* child = [children lastObject];
    if (![child isLeaf]) {
      NSOutlineView* outlineView = [notif object];
      // Tell the OutlineView to expand the NSTreeNode, not the model object.
      children = [item childNodes];
      DCHECK_EQ([children count], 1U);
      [outlineView expandItem:[children lastObject]];
      // Select the first node in that child set.
      NSTreeNode* folderChild = [children lastObject];
      if ([[folderChild childNodes] count] > 0) {
        NSTreeNode* firstCookieChild =
            [[folderChild childNodes] objectAtIndex:0];
        [treeController_ setSelectionIndexPath:[firstCookieChild indexPath]];
      }
    }
  }
}

- (void)outlineViewSelectionDidChange:(NSNotification*)notif {
  // Multi-selection should be disabled in the UI, but for sanity, double-check
  // that they can't do it here.
  NSArray* selectedObjects = [treeController_ selectedObjects];
  NSUInteger count = [selectedObjects count];
  if (count != 1U) {
    DCHECK_LT(count, 1U) << "User was able to select more than 1 cookie node!";
    [self setRemoveButtonEnabled:NO];
    return;
  }

  // Go through the selection's indexPath and make sure that the node that is
  // being referenced actually exists in the Cocoa model.
  NSIndexPath* selection = [treeController_ selectionIndexPath];
  NSUInteger length = [selection length];
  CocoaCookieTreeNode* node = [self cocoaTreeModel];
  for (NSUInteger i = 0; i < length; ++i) {
    NSUInteger childIndex = [selection indexAtPosition:i];
    if (childIndex >= [[node children] count]) {
      [self setRemoveButtonEnabled:NO];
      return;
    }
    node = [[node children] objectAtIndex:childIndex];
  }

  // If there is a valid selection, make sure that the remove
  // button is enabled.
  [self setRemoveButtonEnabled:YES];
}

#pragma mark Unit Testing

- (CookiesTreeModelObserverBridge*)modelObserver {
  return modelObserver_.get();
}

- (NSArray*)icons {
  return icons_.get();
}

// Re-initializes the |treeModel_|, creates a new observer for it, and re-
// builds the |cocoaTreeModel_|. We use this to initialize the controller and
// to rebuild after the user clears browsing data. Because the models get
// clobbered, we rebuild the icon cache for safety (though they do not change).
- (void)loadTreeModelFromProfile {
  treeModel_.reset(new CookiesTreeModel(
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster(),
      databaseHelper_,
      storageHelper_,
      NULL,
      appcacheHelper_,
      indexedDBHelper_));
  modelObserver_.reset(new CookiesTreeModelObserverBridge(self));
  treeModel_->AddObserver(modelObserver_.get());

  // Convert the model's icons from Skia to Cocoa.
  std::vector<SkBitmap> skiaIcons;
  treeModel_->GetIcons(&skiaIcons);
  icons_.reset([[NSMutableArray alloc] init]);
  for (std::vector<SkBitmap>::iterator it = skiaIcons.begin();
       it != skiaIcons.end(); ++it) {
    [icons_ addObject:gfx::SkBitmapToNSImage(*it)];
  }

  // Default icon will be the last item in the array.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // TODO(rsesek): Rename this resource now that it's in multiple places.
  [icons_ addObject:rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER)];

  // Create the Cocoa model.
  CookieTreeNode* root = static_cast<CookieTreeNode*>(treeModel_->GetRoot());
  scoped_nsobject<CocoaCookieTreeNode> model(
      [[CocoaCookieTreeNode alloc] initWithNode:root]);
  [self setCocoaTreeModel:model.get()];  // Takes ownership.
}

@end
