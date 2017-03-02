// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_

#include <memory>
#include <vector>

#include "base/macros.h"

namespace views {
class ImageView;
class View;
}

namespace payments {

class PaymentRequest;

// A control representing a list of selectable items in the PaymentRequest
// dialog. These lists enforce that only one of their elements be selectable at
// a time and that "incomplete" items (for example, a credit card with no known
// expiration date) behave differently when selected. Most of the time, this
// behavior is to show an editor screen.
class PaymentRequestItemList {
 public:
  // Represents an item in the item list.
  class Item {
   public:
    // Creates an item that will be owned by |list| with the initial state set
    // to |selected|.
    Item(PaymentRequest* request, PaymentRequestItemList* list, bool selected);
    virtual ~Item();

    // Gets the view associated with this item. It's owned by this object so
    // that it can listen to any changes to the underlying model and update the
    // view.
    views::View* GetItemView();

    bool selected() const { return selected_; }
    // Changes the selected state of this item to |selected| and calls
    // SelectedStateChanged.
    void SetSelected(bool selected);

    // Returns a pointer to the PaymentRequestItemList that owns this object.
    PaymentRequestItemList* list() { return list_; }

    // Returns a pointer to the PaymentRequest object associated with this
    // instance of the UI.
    PaymentRequest* request() { return request_; }

   protected:
    // Creates and returns the view associated with this list item.
    virtual std::unique_ptr<views::View> CreateItemView() = 0;

    // Called when the selected state of this item changes. Subclasses may
    // assume that they are the only selected item in |list| when this is
    // called. This could be called before CreateItemView so subclasses should
    // be aware that their views might not exist yet.
    virtual void SelectedStateChanged() = 0;

    // Creates an image of a large checkmark, used to indicate that an option is
    // selected.
    std::unique_ptr<views::ImageView> CreateCheckmark(bool selected);

   private:
    std::unique_ptr<views::View> item_view_;
    PaymentRequest* request_;
    PaymentRequestItemList* list_;
    bool selected_;

    DISALLOW_COPY_AND_ASSIGN(Item);
  };

  PaymentRequestItemList();
  ~PaymentRequestItemList();

  // Adds an item to this list. |item->list()| should return this object.
  void AddItem(std::unique_ptr<Item> item);

  // Creates and returns the UI representation of this list. It iterates over
  // the items it contains, creates their associated views, and adds them to the
  // hierarchy.
  std::unique_ptr<views::View> CreateListView();

  // Deselects the currently selected item and selects |item| instead.
  void SelectItem(Item* item);

 private:
  // Unselects the currently selected item. This is private so that the list can
  // use it when selecting a new item while avoiding consumers of this class
  // putting the list in a state where no item is selected.
  void UnselectSelectedItem();

  std::vector<std::unique_ptr<Item>> items_;
  Item* selected_item_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestItemList);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
