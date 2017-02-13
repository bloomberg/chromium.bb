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
  // The NavigationItem passed on initialization.
  web::NavigationItemImpl* _item;
}

@end

@implementation CRWSessionEntry

- (instancetype)initWithNavigationItem:(web::NavigationItem*)item {
  self = [super init];
  if (self) {
    DCHECK(item);
    _item = static_cast<web::NavigationItemImpl*>(item);
  }
  return self;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"url:%@ originalurl:%@ title:%@ transition:%d displayState:%@ "
          @"desktopUA:%d",
          base::SysUTF8ToNSString(_item->GetURL().spec()),
          base::SysUTF8ToNSString(_item->GetOriginalRequestURL().spec()),
          base::SysUTF16ToNSString(_item->GetTitle()),
          _item->GetTransitionType(),
          _item->GetPageDisplayState().GetDescription(),
          _item->IsOverridingUserAgent()];
}

- (web::NavigationItem*)navigationItem {
  return _item;
}

- (web::NavigationItemImpl*)navigationItemImpl {
  return _item;
}

- (BOOL)isEqual:(CRWSessionEntry*)object {
  return _item == [object navigationItem];
}

@end
