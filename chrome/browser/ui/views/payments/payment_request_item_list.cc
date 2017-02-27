// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace payments {

PaymentRequestItemList::Item::Item(PaymentRequest* request,
                                   PaymentRequestItemList* list,
                                   bool selected)
    : request_(request), list_(list), selected_(selected) {}

PaymentRequestItemList::Item::~Item() {}

views::View* PaymentRequestItemList::Item::GetItemView() {
  if (!item_view_) {
    item_view_ = CreateItemView();
    item_view_->set_owned_by_client();
  }

  return item_view_.get();
}

void PaymentRequestItemList::Item::SetSelected(bool selected) {
  selected_ = selected;
  SelectedStateChanged();
}

PaymentRequestItemList::PaymentRequestItemList() : selected_item_(nullptr) {}

PaymentRequestItemList::~PaymentRequestItemList() {}

void PaymentRequestItemList::AddItem(
    std::unique_ptr<PaymentRequestItemList::Item> item) {
  DCHECK_EQ(this, item->list());
  items_.push_back(std::move(item));
  if (items_.back()->selected())
    SelectItem(items_.back().get());
}

std::unique_ptr<views::View> PaymentRequestItemList::CreateListView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, kPaymentRequestRowVerticalInsets, 0);
  content_view->SetLayoutManager(layout);

  for (auto& item : items_) {
    content_view->AddChildView(item->GetItemView());
  }

  return content_view;
}

void PaymentRequestItemList::SelectItem(PaymentRequestItemList::Item* item) {
  DCHECK_EQ(this, item->list());
  if (selected_item_ == item)
    return;

  UnselectSelectedItem();

  selected_item_ = item;
  item->SetSelected(true);
}

void PaymentRequestItemList::UnselectSelectedItem() {
  // It's possible that no item is currently selected, either during list
  // creation or in the middle of the selection operation when the previously
  // selected item has been deselected but the new one isn't selected yet.
  if (selected_item_)
    selected_item_->SetSelected(false);

  selected_item_ = nullptr;
}

}  // namespace payments
