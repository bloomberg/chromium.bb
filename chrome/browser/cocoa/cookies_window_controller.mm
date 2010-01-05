// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookies_window_controller.h"

#include <vector>

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/i18n/time_formatting.h"
#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/apple/ImageAndTextCell.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Key path used for notifying KVO.
static NSString* const kCocoaTreeModel = @"cocoaTreeModel";

CookiesTreeModelObserverBridge::CookiesTreeModelObserverBridge(
    CookiesWindowController* controller)
    : window_controller_(controller) {
}

// Notification that nodes were added to the specified parent.
void CookiesTreeModelObserverBridge::TreeNodesAdded(TreeModel* model,
                                                   TreeModelNode* parent,
                                                   int start,
                                                   int count) {
  CocoaCookieTreeNode* cocoa_parent = FindCocoaNode(parent, nil);
  NSMutableArray* cocoa_children = [cocoa_parent children];

  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  CookieTreeNode* cookie_parent = static_cast<CookieTreeNode*>(parent);
  for (int i = 0; i < count; ++i) {
    CookieTreeNode* cookie_child = cookie_parent->GetChild(start + i);
    CocoaCookieTreeNode* new_child = CocoaNodeFromTreeNode(cookie_child, true);
    [cocoa_children addObject:new_child];
  }
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

// Notification that nodes were removed from the specified parent.
void CookiesTreeModelObserverBridge::TreeNodesRemoved(TreeModel* model,
                                                     TreeModelNode* parent,
                                                     int start,
                                                     int count) {
  CocoaCookieTreeNode* cocoa_parent = FindCocoaNode(parent, nil);
  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  NSMutableArray* cocoa_children = [cocoa_parent children];
  for (int i = start + count - 1; i >= start; --i) {
    [cocoa_children removeObjectAtIndex:i];
  }
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

// Notification the children of |parent| have been reordered. Note, only
// the direct children of |parent| have been reordered, not descendants.
void CookiesTreeModelObserverBridge::TreeNodeChildrenReordered(TreeModel* model,
    TreeModelNode* parent) {
  CocoaCookieTreeNode* cocoa_parent = FindCocoaNode(parent, nil);
  NSMutableArray* cocoa_children = [cocoa_parent children];

  CookieTreeNode* cookie_parent = static_cast<CookieTreeNode*>(parent);
  const int child_count = cookie_parent->GetChildCount();

  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  for (int i = 0; i < child_count; ++i) {
    CookieTreeNode* swap_in = cookie_parent->GetChild(i);
    for (int j = i; j < child_count; ++j) {
      CocoaCookieTreeNode* child = [cocoa_children objectAtIndex:j];
      TreeModelNode* swap_out = [child treeNode];
      if (swap_in == swap_out) {
        [cocoa_children exchangeObjectAtIndex:j withObjectAtIndex:i];
        break;
      }
    }
  }
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

// Notification that the contents of a node has changed.
void CookiesTreeModelObserverBridge::TreeNodeChanged(TreeModel* model,
                                                    TreeModelNode* node) {
  [window_controller_ willChangeValueForKey:kCocoaTreeModel];
  CocoaCookieTreeNode* changed_node = FindCocoaNode(node, nil);
  [changed_node rebuild];
  [window_controller_ didChangeValueForKey:kCocoaTreeModel];
}

CocoaCookieTreeNode* CookiesTreeModelObserverBridge::CocoaNodeFromTreeNode(
    TreeModelNode* node, bool recurse) {
  CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
  return [[[CocoaCookieTreeNode alloc] initWithNode:cookie_node] autorelease];
}

// Does a pre-order traversal on the tree to find |node|.
CocoaCookieTreeNode* CookiesTreeModelObserverBridge::FindCocoaNode(
    TreeModelNode* node, CocoaCookieTreeNode* start) {
  if (!start) {
    start = [window_controller_ cocoaTreeModel];
  }
  if ([start treeNode] == node) {
    return start;
  }

  NSArray* children = [start children];
  for (CocoaCookieTreeNode* child in children) {
    if ([child treeNode] == node) {
      return child;
    }

    // Search the children. Return the result if we find one.
    CocoaCookieTreeNode* recurse = FindCocoaNode(node, child);
    if (recurse)
      return recurse;
  }
  return nil;  // We couldn't find the node.
}

#pragma mark Window Controller

@implementation CookiesWindowController

@synthesize treeController = treeController_;

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath = [mac_util::MainAppBundle() pathForResource:@"Cookies"
                                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    treeModel_.reset(new CookiesTreeModel(profile_));
    modelObserver_.reset(new CookiesTreeModelObserverBridge(self));
    treeModel_->SetObserver(modelObserver_.get());

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
    [icons_ addObject:rb.GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER)];

    // Create the Cocoa model.
    CookieTreeNode* root = static_cast<CookieTreeNode*>(treeModel_->GetRoot());
    cocoaTreeModel_.reset([[CocoaCookieTreeNode alloc] initWithNode:root]);
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
}

- (void)windowWillClose:(NSNotification*)notif {
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

- (IBAction)deleteCookie:(id)sender {
  scoped_nsobject<NSArray> selection(
      [[treeController_ selectedObjects] retain]);
  for (CocoaCookieTreeNode* node in selection.get()) {
    CookieTreeNode* cookie = static_cast<CookieTreeNode*>([node treeNode]);
    treeModel_->DeleteCookieNode(cookie);
  }
}

- (IBAction)deleteAllCookies:(id)sender {
  treeModel_->DeleteAllCookies();
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

#pragma mark Getters and Setters

- (CocoaCookieTreeNode*)cocoaTreeModel {
  return cocoaTreeModel_.get();
}
- (void)setCocoaTreeModel:(CocoaCookieTreeNode*)model {
  return cocoaTreeModel_.reset([model retain]);
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

#pragma mark Unit Testing

- (CookiesTreeModelObserverBridge*)modelObserver {
  return modelObserver_.get();
}

- (NSArray*)icons {
  return icons_.get();
}

@end
