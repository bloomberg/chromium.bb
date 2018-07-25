// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NUX_SHOW_PROMO_DELEGATE_H_
#define COMPONENTS_NUX_SHOW_PROMO_DELEGATE_H_

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

class ShowPromoDelegate {
 public:
  // Shows a promotional popup for the specified bookmark node.
  virtual void ShowForNode(const bookmarks::BookmarkNode* node) = 0;

 protected:
  virtual ~ShowPromoDelegate() = default;
};

#endif  //  COMPONENTS_NUX_SHOW_PROMO_DELEGATE_H_
