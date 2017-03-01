// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/contact_info_view_controller.h"

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"

namespace payments {

namespace {

class ContactInfoListItem : public payments::PaymentRequestItemList::Item,
                            public views::ButtonListener {
 public:
  // Constructs a ContactInfoListItem object owned by |list|. |request| is the
  // PaymentRequest object that is represented by the current instance of the
  // dialog. |profile| is the AutofillProfile that this specific list item
  // represents. It's a cached profile owned by |request|.
  ContactInfoListItem(autofill::AutofillProfile* profile,
                      PaymentRequest* request,
                      PaymentRequestItemList* list,
                      bool selected)
      : payments::PaymentRequestItemList::Item(request, list, selected),
        profile_(profile) {}
  ~ContactInfoListItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateItemView() override {
    std::unique_ptr<PaymentRequestRowView> row =
        base::MakeUnique<PaymentRequestRowView>(this);
    views::GridLayout* layout = new views::GridLayout(row.get());
    layout->SetInsets(
        kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
        kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets);
    row->SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);

    // A column for the contact info
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                       views::GridLayout::USE_PREF, 0, 0);
    // A padding column that resizes to take up the empty space between the
    // leading and trailing parts.
    columns->AddPaddingColumn(1, 0);

    // A column for the checkmark when the row is selected.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    // TODO(anthonyvd): derive the following boolean options from the requested
    // options in the PaymentRequest object.
    std::unique_ptr<views::View> contact_info_label = GetContactInfoLabel(
        AddressStyleType::DETAILED, std::string(), *profile_, true, true, true);
    contact_info_label->set_can_process_events_within_subtree(false);
    layout->AddView(contact_info_label.release());

    checkmark_ = base::MakeUnique<views::ImageView>();
    checkmark_->set_id(
        static_cast<int>(DialogViewID::CONTACT_INFO_ITEM_CHECKMARK_VIEW));
    checkmark_->set_owned_by_client();
    checkmark_->set_can_process_events_within_subtree(false);
    checkmark_->SetImage(
        gfx::CreateVectorIcon(views::kMenuCheckIcon, 0xFF609265));
    layout->AddView(checkmark_.get());
    if (!selected())
      checkmark_->SetVisible(false);

    return std::move(row);
  }

  // payments::PaymentRequestItemList::Item:
  void SelectedStateChanged() override {}

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}

  autofill::AutofillProfile* profile_;
  std::unique_ptr<views::ImageView> checkmark_;

  DISALLOW_COPY_AND_ASSIGN(ContactInfoListItem);
};

}  // namespace

ContactInfoViewController::ContactInfoViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {
  const std::vector<autofill::AutofillProfile*>& profiles =
      request->contact_profiles();
  for (autofill::AutofillProfile* profile : profiles) {
    std::unique_ptr<ContactInfoListItem> item =
        base::MakeUnique<ContactInfoListItem>(
            profile, request, &contact_info_list_,
            profile == request->selected_contact_profile());
    contact_info_list_.AddItem(std::move(item));
  }
}

ContactInfoViewController::~ContactInfoViewController() {}

std::unique_ptr<views::View> ContactInfoViewController::CreateView() {
  std::unique_ptr<views::View> list_view = contact_info_list_.CreateListView();

  return CreatePaymentView(
      CreateSheetHeaderView(true,
                            l10n_util::GetStringUTF16(
                                IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME),
                            this),
      std::move(list_view));
}

}  // namespace payments
