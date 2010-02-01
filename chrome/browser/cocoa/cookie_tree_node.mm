// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookie_tree_node.h"

#include "app/l10n_util_mac.h"
#import "base/i18n/time_formatting.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"

@implementation CocoaCookieTreeNode

- (id)initWithNode:(CookieTreeNode*)node {
  if ((self = [super init])) {
    DCHECK(node);
    treeNode_ = node;
    isLeaf_ = (node->GetChildCount() == 0);

    [self rebuild];
  }
  return self;
}

- (void)rebuild {
  title_.reset([base::SysWideToNSString(treeNode_->GetTitle()) retain]);
  isCookie_ = NO;

  CookieTreeNode::DetailedInfo info = treeNode_->GetDetailedInfo();
  if (info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
    isCookie_ = YES;
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
  }
}

- (BOOL)isLeaf {
  return isLeaf_;
}

- (NSString*)title {
  return title_.get();
}

- (NSMutableArray*)children {
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

- (TreeModelNode*)treeNode {
  return treeNode_;
}

#pragma mark Cookie Accessors

- (BOOL)isCookie {
  return isCookie_;
}

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

@end
