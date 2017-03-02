// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_list_view_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

class ShippingListItem : public payments::PaymentRequestItemList::Item,
                         public views::ButtonListener {
 public:
  ShippingListItem(autofill::AutofillProfile* profile,
                   PaymentRequest* request,
                   PaymentRequestItemList* list,
                   bool selected)
      : payments::PaymentRequestItemList::Item(request, list, selected),
        profile_(profile) {}
  ~ShippingListItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateItemView() override {
    DCHECK(profile_);

    // TODO(tmartino): Pass an actual locale in place of empty string.
    std::unique_ptr<views::View> content = GetShippingAddressLabel(
        AddressStyleType::DETAILED, std::string(), *profile_);

    std::unique_ptr<PaymentRequestRowView> row =
        base::MakeUnique<PaymentRequestRowView>(this);
    views::GridLayout* layout = new views::GridLayout(row.get());
    row->SetLayoutManager(layout);

    layout->SetInsets(
        kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
        kPaymentRequestRowVerticalInsets,
        kPaymentRequestRowHorizontalInsets + kPaymentRequestRowExtraRightInset);

    // Add a column listing the address.
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                       views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(1, 0);

    // Add a column for the checkmark shown next to the selected address.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    content->set_can_process_events_within_subtree(false);
    layout->AddView(content.release());

    checkmark_ = CreateCheckmark(selected());
    layout->AddView(checkmark_.get());

    return std::move(row);
  }

  void SelectedStateChanged() override {}

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}

  autofill::AutofillProfile* profile_;
  std::unique_ptr<views::ImageView> checkmark_;
};

ShippingListViewController::ShippingListViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {
  auto* selected_profile = request->selected_shipping_profile();

  for (auto* profile : request->shipping_profiles()) {
    list_.AddItem(base::MakeUnique<ShippingListItem>(
        profile, request, &list_, profile == selected_profile));
  }
}

ShippingListViewController::~ShippingListViewController() {}

std::unique_ptr<views::View> ShippingListViewController::CreateView() {
  return CreatePaymentView(
      CreateSheetHeaderView(
          /* show_back_arrow = */ true,
          l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_SHIPPING_SECTION_NAME),
          this),
      list_.CreateListView());
}

}  // namespace payments
