// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_

#include <memory>
#include <vector>

#include "base/macros.h"

namespace views {
class View;
}

namespace payments {

// A control representing a list of selectable items in the PaymentRequest
// dialog. These lists enforce that only one of their elements be selectable at
// a time and that "incomplete" items (for example, a credit card with no known
// expriration date) behave differently when selected. Most of the time, this
// behavior is to show an editor screen.
class PaymentRequestItemList {
 public:
  // Represents an item in the item list.
  class Item {
   public:
    Item();
    virtual ~Item();

    // Gets the view associated with this item. It's owned by this object so
    // that it can listen to any changes to the underlying model and update the
    // view.
    views::View* GetItemView();

   protected:
    // Creates and returns the view associated with this list item.
    virtual std::unique_ptr<views::View> CreateItemView() = 0;

   private:
    std::unique_ptr<views::View> item_view_;

    DISALLOW_COPY_AND_ASSIGN(Item);
  };

  PaymentRequestItemList();
  ~PaymentRequestItemList();

  // Adds an item to this list.
  void AddItem(std::unique_ptr<Item> item);

  // Creates and returns the UI representation of this list. It iterates over
  // the items it contains, creates their associated views, and adds them to the
  // hierarchy.
  std::unique_ptr<views::View> CreateListView();

 private:
  std::vector<std::unique_ptr<Item>> items_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestItemList);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
