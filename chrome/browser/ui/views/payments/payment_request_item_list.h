// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class View;
}

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;

// A control representing a list of selectable items in the PaymentRequest
// dialog. These lists enforce that only one of their elements be selectable at
// a time and that "incomplete" items (for example, a credit card with no known
// expiration date) behave differently when selected. Most of the time, this
// behavior is to show an editor screen.
class PaymentRequestItemList {
 public:
  // Represents an item in the item list.
  class Item : public views::ButtonListener {
   public:
    // Creates an item that will be owned by |list| with the initial state set
    // to |selected|.
    Item(PaymentRequestSpec* spec,
         PaymentRequestState* state,
         PaymentRequestItemList* list,
         bool selected,
         bool show_edit_button);
    ~Item() override;

    bool selected() const { return selected_; }
    // Changes the selected state of this item to |selected|.
    // SelectedStateChanged is called if |notify| is true.
    void SetSelected(bool selected, bool notify);

    // Creates and returns the view associated with this list item.
    std::unique_ptr<views::View> CreateItemView();

    // Returns a pointer to the PaymentRequestItemList that owns this object.
    PaymentRequestItemList* list() { return list_; }

    // Returns a pointer to the PaymentRequestSpec/State objects associated with
    // this instance of the UI.
    PaymentRequestSpec* spec() { return spec_; }
    PaymentRequestState* state() { return state_; }

   protected:
    // Called when the selected state of this item changes. Subclasses may
    // assume that they are the only selected item in |list| when this is
    // called. This could be called before CreateItemView so subclasses should
    // be aware that their views might not exist yet.
    virtual void SelectedStateChanged() = 0;

    // Creates an image of a large checkmark, used to indicate that an option is
    // selected.
    std::unique_ptr<views::ImageView> CreateCheckmark(bool selected);

    // Creates the view that represents this item's content. Typically this will
    // be a label describing the payment method, shipping adress, etc.
    virtual std::unique_ptr<views::View> CreateContentView() = 0;

    // Creates the view that should be displayed after the checkmark in the
    // item's view, such as the credit card icon.
    virtual std::unique_ptr<views::View> CreateExtraView();

    // Whether the item should be enabled (if disabled, the user will not be
    // able to click on the item).
    virtual bool IsEnabled() = 0;

    // Returns whether this item is complete/valid and can be selected by the
    // user. If this returns false when the user attempts to select this item,
    // PerformSelectionFallback will be called instead.
    virtual bool CanBeSelected() = 0;

    // Performs the action that replaces selection when CanBeSelected returns
    // false. This will usually be to display an editor.
    virtual void PerformSelectionFallback() = 0;

    // Called when the edit button is pressed. Subclasses should open the editor
    // appropriate for the item they represent.
    virtual void EditButtonPressed() = 0;

   private:
    // views::ButtonListener:
    void ButtonPressed(views::Button* sender, const ui::Event& event) override;

    PaymentRequestSpec* spec_;
    PaymentRequestState* state_;
    PaymentRequestItemList* list_;
    std::unique_ptr<views::ImageView> checkmark_;
    bool selected_;
    bool show_edit_button_;

    DISALLOW_COPY_AND_ASSIGN(Item);
  };

  PaymentRequestItemList();
  virtual ~PaymentRequestItemList();

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
