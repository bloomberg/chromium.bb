// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"

#import "ios/web/public/navigation_item.h"

namespace web {

namespace {
// Private key used for safe conversion of base::SupportsUserData to
// web::WKBackForwardListItemHolder in
// web::WKBackForwardListItemHolder::FromNavigationItem.
const char kBackForwardListItemIdentifierKey[] =
    "BackForwardListItemIdentifierKey";
}

WKBackForwardListItemHolder::WKBackForwardListItemHolder()
    : navigation_type_(WKNavigationTypeOther) {}

WKBackForwardListItemHolder::~WKBackForwardListItemHolder() {}

// static
WKBackForwardListItemHolder* WKBackForwardListItemHolder::FromNavigationItem(
    web::NavigationItem* item) {
  DCHECK(item);
  base::SupportsUserData::Data* user_data =
      item->GetUserData(kBackForwardListItemIdentifierKey);
  if (!user_data) {
    user_data = new WKBackForwardListItemHolder();
    item->SetUserData(kBackForwardListItemIdentifierKey, user_data);
  }
  return static_cast<WKBackForwardListItemHolder*>(user_data);
}

}  // namespace web
