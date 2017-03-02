// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_entry.h"

#include <stdint.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_state/page_display_state.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWSessionEntry () {
  // The NavigationItemImpl corresponding to this CRWSessionEntry.
  // TODO(stuartmorgan): Move ownership to NavigationManagerImpl.
  std::unique_ptr<web::NavigationItemImpl> _navigationItem;
}

@end

@implementation CRWSessionEntry

- (instancetype)initWithNavigationItem:
    (std::unique_ptr<web::NavigationItem>)item {
  self = [super init];
  if (self) {
    _navigationItem.reset(
        static_cast<web::NavigationItemImpl*>(item.release()));
  }
  return self;
}

// TODO(ios): Shall we overwrite EqualTo:?

- (instancetype)copyWithZone:(NSZone*)zone {
  CRWSessionEntry* copy = [[[self class] alloc] init];
  copy->_navigationItem.reset(
      new web::NavigationItemImpl(*_navigationItem.get()));
  return copy;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"url:%@ originalurl:%@ title:%@ transition:%d displayState:%@ "
          @"userAgentType:%s",
          base::SysUTF8ToNSString(_navigationItem->GetURL().spec()),
          base::SysUTF8ToNSString(
              _navigationItem->GetOriginalRequestURL().spec()),
          base::SysUTF16ToNSString(_navigationItem->GetTitle()),
          _navigationItem->GetTransitionType(),
          _navigationItem->GetPageDisplayState().GetDescription(),
          web::GetUserAgentTypeDescription(_navigationItem->GetUserAgentType())
              .c_str()];
}

- (web::NavigationItem*)navigationItem {
  return _navigationItem.get();
}

- (web::NavigationItemImpl*)navigationItemImpl {
  return _navigationItem.get();
}

@end
