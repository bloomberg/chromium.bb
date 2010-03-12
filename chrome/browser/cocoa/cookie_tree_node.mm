// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookie_tree_node.h"

#include "app/l10n_util_mac.h"
#import "base/i18n/time_formatting.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "grit/generated_resources.h"

@implementation CocoaCookieTreeNode

- (id)initWithNode:(CookieTreeNode*)node {
  if ((self = [super init])) {
    DCHECK(node);
    treeNode_ = node;
    [self rebuild];
  }
  return self;
}

- (void)rebuild {
  title_.reset([base::SysWideToNSString(treeNode_->GetTitle()) retain]);
  children_.reset();
  nodeType_ = kCocoaCookieTreeNodeTypeFolder;

  CookieTreeNode::DetailedInfo info = treeNode_->GetDetailedInfo();
  CookieTreeNode::DetailedInfo::NodeType nodeType = info.node_type;
  if (nodeType == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
    nodeType_ = kCocoaCookieTreeNodeTypeCookie;
    net::CookieMonster::CanonicalCookie cookie = info.cookie->second;

    name_.reset([base::SysUTF8ToNSString(cookie.Name()) retain]);
    title_.reset([base::SysUTF8ToNSString(cookie.Name()) retain]);
    content_.reset([base::SysUTF8ToNSString(cookie.Value()) retain]);
    path_.reset([base::SysUTF8ToNSString(cookie.Path()) retain]);
    domain_.reset([base::SysWideToNSString(info.origin) retain]);

    if (cookie.DoesExpire()) {
      expires_.reset([base::SysWideToNSString(
          base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())) retain]);
    } else {
      expires_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_EXPIRES_SESSION) retain]);
    }

    created_.reset([base::SysWideToNSString(
        base::TimeFormatFriendlyDateAndTime(cookie.CreationDate())) retain]);

    if (cookie.IsSecure()) {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_SECURE) retain]);
    } else {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_ANY) retain]);
    }
  } else if (nodeType == CookieTreeNode::DetailedInfo::TYPE_DATABASE) {
    const BrowsingDataDatabaseHelper::DatabaseInfo* databaseInfo =
        info.database_info;
    nodeType_ = kCocoaCookieTreeNodeTypeDatabaseStorage;
    databaseDescription_.reset([base::SysUTF8ToNSString(
        databaseInfo->description) retain]);
    fileSize_.reset([base::SysWideToNSString(FormatBytes(databaseInfo->size,
        GetByteDisplayUnits(databaseInfo->size), true)) retain]);
    lastModified_.reset([base::SysWideToNSString(
        base::TimeFormatFriendlyDateAndTime(
            databaseInfo->last_modified)) retain]);
  } else if (nodeType == CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE) {
    const BrowsingDataLocalStorageHelper::LocalStorageInfo* storageInfo =
        info.local_storage_info;
    nodeType_ = kCocoaCookieTreeNodeTypeLocalStorage;
    domain_.reset([base::SysUTF8ToNSString(storageInfo->origin) retain]);
    fileSize_.reset([base::SysWideToNSString(FormatBytes(storageInfo->size,
        GetByteDisplayUnits(storageInfo->size), true)) retain]);
    lastModified_.reset([base::SysWideToNSString(
        base::TimeFormatFriendlyDateAndTime(
            storageInfo->last_modified)) retain]);
  }
}

- (NSString*)title {
  return title_.get();
}

- (CocoaCookieTreeNodeType)nodeType {
  return nodeType_;
}

- (TreeModelNode*)treeNode {
  return treeNode_;
}

- (NSMutableArray*)mutableChildren {
  if (!children_.get()) {
    const int childCount = treeNode_->GetChildCount();
    children_.reset([[NSMutableArray alloc] initWithCapacity:childCount]);
    for (int i = 0; i < childCount; ++i) {
      CookieTreeNode* child = treeNode_->GetChild(i);
      scoped_nsobject<CocoaCookieTreeNode> childNode(
          [[CocoaCookieTreeNode alloc] initWithNode:child]);
      [children_ addObject:childNode.get()];
    }
  }
  return children_.get();
}

- (NSArray*)children {
  return [self mutableChildren];
}

- (BOOL)isLeaf {
  return nodeType_ != kCocoaCookieTreeNodeTypeFolder;
}

- (NSString*)description {
  NSString* format =
      @"<CocoaCookieTreeNode @ %p (title=%@, nodeType=%d, childCount=%u)";
  return [NSString stringWithFormat:format, self, [self title],
      [self nodeType], [[self children] count]];
}

- (BOOL)isFolderOrCookieTreeDetails {
  return [self nodeType] == kCocoaCookieTreeNodeTypeFolder ||
      [self nodeType] == kCocoaCookieTreeNodeTypeCookie;
}

- (BOOL)isDatabaseTreeDetails {
  return [self nodeType] == kCocoaCookieTreeNodeTypeDatabaseStorage;
}

- (BOOL)isLocalStorageTreeDetails {
  return [self nodeType] == kCocoaCookieTreeNodeTypeLocalStorage;
}

- (BOOL)isDatabasePromptDetails {
  return false;
}

- (BOOL)isLocalStoragePromptDetails {
  return false;
}

#pragma mark Cookie Accessors

- (NSString*)name {
  return name_.get();
}

- (NSString*)content {
  return content_.get();
}

- (NSString*)domain {
  return domain_.get();
}

- (NSString*)path {
  return path_.get();
}

- (NSString*)sendFor {
  return sendFor_.get();
}

- (NSString*)created {
  return created_.get();
}

- (NSString*)expires {
  return expires_.get();
}

#pragma mark Local Storage and Database Accessors

- (NSString*)fileSize {
  return fileSize_.get();
}

- (NSString*)lastModified {
  return lastModified_.get();
}

#pragma mark Database Accessors

- (NSString*)databaseDescription {
  return databaseDescription_.get();
}

#pragma mark Unused Accessors

// This method is never called for the cookie tree, it is only
// only included because the Cocoa bindings for the shared view
// used to display browser data details always expects the method
// even though it is only used in the cookie prompt window.
- (id)localStorageKey {
  NOTIMPLEMENTED();
  return nil;
}

// This method is never called for the cookie tree, it is only
// only included because the Cocoa bindings for the shared view
// used to display browser data details always expects the method
// even though it is only used in the cookie prompt window.
- (id)localStorageValue {
  NOTIMPLEMENTED();
  return nil;
}

@end
