// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cookies_tree_model.h"

// This class is used by CookiesWindowController and represents a node in the
// cookie tree view.
@interface CocoaCookieTreeNode : NSObject {
  scoped_nsobject<NSString> title_;
  scoped_nsobject<NSMutableArray> children_;

  // The platform-independent model node.
  CookieTreeNode* treeNode_;  // weak

  // These members are only set for true cookie nodes.
  BOOL isCookie_;
  scoped_nsobject<NSString> name_;
  scoped_nsobject<NSString> content_;
  scoped_nsobject<NSString> domain_;
  scoped_nsobject<NSString> path_;
  scoped_nsobject<NSString> sendFor_;
  // Stringifed dates.
  scoped_nsobject<NSString> created_;
  scoped_nsobject<NSString> expires_;
}

// Designated initializer.
- (id)initWithNode:(CookieTreeNode*)node;

// Re-sets all the members of the node based on |treeNode_|.
- (void)rebuild;

- (BOOL)isLeaf;

// Getters.
- (NSString*)title;
// |-children| is mutable so that the CookiesTreeModelObserverBridge can
// operate on the children.
- (NSMutableArray*)children;
- (TreeModelNode*)treeNode;

// Used only by cookies. Nil for non-cookie nodes.
- (BOOL)isCookie;
- (NSString*)name;
- (NSString*)content;
- (NSString*)domain;
- (NSString*)path;
- (NSString*)sendFor;
- (NSString*)created;
- (NSString*)expires;

@end
