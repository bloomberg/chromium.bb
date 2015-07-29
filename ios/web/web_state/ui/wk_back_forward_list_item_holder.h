// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_
#define IOS_WEB_WEB_STATE_UI_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_

#import <WebKit/WebKit.h>

#include "base/ios/weak_nsobject.h"
#include "base/macros.h"
#include "base/supports_user_data.h"

namespace web {

class NavigationItem;

// This class is a wrapper for information needed to implement native
// WKWebView navigation. WKBackForwardListItemHolder is attached to
// NavigationItem via the SupportsUserData interface and holds two
// values:
// (1) the NavigationItem's corresponding WKBackForwardListItem,
// (2) whether or not the NavigationItem's HTTP method is POST
class WKBackForwardListItemHolder : public base::SupportsUserData::Data {
 public:
  // Returns the WKBackForwardListItemHolder for the NavigationItem |item|.
  // Lazily attaches one if it does not exist. |item| cannot be null.
  static web::WKBackForwardListItemHolder* FromNavigationItem(
      NavigationItem* item);

  // Accessors for |item_|. Use these to get/set the association between a
  // NavigationItem and a WKBackForwardListItem. Note that
  // |GetBackForwardListItem| may return nil (f.e. when the
  // parent WKBackForwardList is deallocated).
  WKBackForwardListItem* back_forward_list_item() { return item_; }
  void set_back_forward_list_item(WKBackForwardListItem* item) {
    item_.reset(item);
  }

  // Accessors for |is_post_|. true if the associated Navigationitem's
  // HTTP method is POST.
  bool is_post() { return is_post_; }
  void set_is_post(bool is_post) { is_post_ = is_post; }

 private:
  WKBackForwardListItemHolder();
  ~WKBackForwardListItemHolder() override;

  // Weak pointer to a WKBackForwardListItem. Becomes nil if the parent
  // WKBackForwardList is deallocated.
  base::WeakNSObject<WKBackForwardListItem> item_;

  // true if the associated Navigationitem's HTTP method is POST.
  bool is_post_;

  DISALLOW_COPY_AND_ASSIGN(WKBackForwardListItemHolder);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_
