// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/ui/cocoa/content_settings/cookie_details.h"

@interface CocoaCookieTreeNode : NSObject {
  scoped_nsobject<NSString> title_;
  scoped_nsobject<NSMutableArray> children_;
  scoped_nsobject<CocoaCookieDetails> details_;
  CookieTreeNode* treeNode_;  // weak
}

// Designated initializer.
- (id)initWithNode:(CookieTreeNode*)node;

// Re-sets all the members of the node based on |treeNode_|.
- (void)rebuild;

// Common getters..
- (NSString*)title;
- (CocoaCookieDetailsType)nodeType;
- (ui::TreeModelNode*)treeNode;

// |-mutableChildren| exists so that the CookiesTreeModelObserverBridge can
// operate on the children. Note that this lazily creates children.
- (NSMutableArray*)mutableChildren;
- (NSArray*)children;
- (BOOL)isLeaf;

- (CocoaCookieDetails*)details;

@end
