// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cookies_tree_model.h"

// This enum specifies the type of display node a CocoaCookieTreeNode is. If
// this system is rewritten to not use bindings, this class should be
// subclassed and specialized, rather than using an enum to determine type.
enum CocoaCookieTreeNodeType {
  // Represents grouping data for the actual data.
  kCocoaCookieTreeNodeTypeFolder = 0,

  // A cookie node.
  kCocoaCookieTreeNodeTypeCookie = 1,

  // A local storage node.
  kCocoaCookieTreeNodeTypeLocalStorage = 2
};

// This class is used by CookiesWindowController and represents a node in the
// cookie tree view.
@interface CocoaCookieTreeNode : NSObject {
  scoped_nsobject<NSString> title_;
  scoped_nsobject<NSMutableArray> children_;

  CocoaCookieTreeNodeType nodeType_;

  // The platform-independent model node.
  CookieTreeNode* treeNode_;  // weak

  // These members are only set for kCocoaCookieTreeNodeTypeCookie nodes.
  scoped_nsobject<NSString> name_;
  scoped_nsobject<NSString> content_;
  scoped_nsobject<NSString> path_;
  scoped_nsobject<NSString> sendFor_;
  // Stringifed dates.
  scoped_nsobject<NSString> created_;
  scoped_nsobject<NSString> expires_;

  // These members are only set for kCocoaCookieTreeNodeTypeLocalStorage nodes.
  scoped_nsobject<NSString> fileSize_;
  scoped_nsobject<NSString> lastModified_;

  // These members are set for both of the two specialized node types.
  scoped_nsobject<NSString> domain_;
}

// Designated initializer.
- (id)initWithNode:(CookieTreeNode*)node;

// Re-sets all the members of the node based on |treeNode_|.
- (void)rebuild;

// Common getters..
- (NSString*)title;
- (CocoaCookieTreeNodeType)nodeType;
- (TreeModelNode*)treeNode;

// |-mutableChildren| exists so that the CookiesTreeModelObserverBridge can
// operate on the children. Note that this lazily creates children.
- (NSMutableArray*)mutableChildren;
- (NSArray*)children;
- (BOOL)isLeaf;

// Used only by kCocoaCookieTreeNodeTypeCookie. Nil for other types.
- (NSString*)name;
- (NSString*)content;
- (NSString*)domain;
- (NSString*)path;
- (NSString*)sendFor;
- (NSString*)created;
- (NSString*)expires;

// Used by kCocoaCookieTreeNodeTypeLocalStorage nodes. Nil for other types.
- (NSString*)fileSize;
- (NSString*)lastModified;

@end
