// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace payments {

PaymentRequestItemList::Item::Item() {}

PaymentRequestItemList::Item::~Item() {}

views::View* PaymentRequestItemList::Item::GetItemView() {
  if (!item_view_) {
    item_view_ = CreateItemView();
    item_view_->set_owned_by_client();
  }

  return item_view_.get();
}

PaymentRequestItemList::PaymentRequestItemList() {}

PaymentRequestItemList::~PaymentRequestItemList() {}

void PaymentRequestItemList::AddItem(
    std::unique_ptr<PaymentRequestItemList::Item> item) {
  items_.push_back(std::move(item));
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

}  // namespace payments
