// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"

namespace payments {

namespace {

constexpr int kFirstTagValue = static_cast<int>(
    payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX);

enum class PaymentMethodViewControllerTags : int {
  // The tag for the button that triggers the "add card" flow. Starts at
  // |kFirstTagValue| not to conflict with tags common to all views.
  ADD_CREDIT_CARD_BUTTON = kFirstTagValue,
};

class PaymentMethodListItem : public payments::PaymentRequestItemList::Item,
                              public views::ButtonListener {
 public:
  // Does not take ownership of |card|, which  should not be null and should
  // outlive this object. |list| is the PaymentRequestItemList object that will
  // own this.
  PaymentMethodListItem(autofill::CreditCard* card,
                        PaymentRequest* request,
                        PaymentRequestItemList* list,
                        bool selected)
      : payments::PaymentRequestItemList::Item(request, list, selected),
        card_(card) {}
  ~PaymentMethodListItem() override {}

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

    // A column for the masked number and name on card
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                       views::GridLayout::USE_PREF, 0, 0);

    // A padding column that resizes to take up the empty space between the
    // leading and trailing parts.
    columns->AddPaddingColumn(1, 0);

    // A column for the checkmark when the row is selected.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(0, kPaymentRequestButtonSpacing);

    // A column for the card icon
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    // A column for the edit button
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    std::unique_ptr<views::View> card_info_container =
        base::MakeUnique<views::View>();
    card_info_container->set_can_process_events_within_subtree(false);

    std::unique_ptr<views::BoxLayout> box_layout =
        base::MakeUnique<views::BoxLayout>(views::BoxLayout::kVertical, 0,
                                           kPaymentRequestRowVerticalInsets, 0);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
    card_info_container->SetLayoutManager(box_layout.release());

    card_info_container->AddChildView(
        new views::Label(card_->TypeAndLastFourDigits()));
    card_info_container->AddChildView(new views::Label(
        card_->GetInfo(autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
                       g_browser_process->GetApplicationLocale())));
    // TODO(anthonyvd): Add the "card is incomplete" label once the
    // completedness logic is implemented.
    layout->AddView(card_info_container.release());

    checkmark_ = CreateCheckmark(selected());
    layout->AddView(checkmark_.get());

    std::unique_ptr<views::ImageView> card_icon_view =
        CreateCardIconView(card_->type());
    card_icon_view->SetImageSize(gfx::Size(32, 20));
    layout->AddView(card_icon_view.release());

    return std::move(row);
  }

  void SelectedStateChanged() override {
    // This could be called before CreateItemView, so before |checkmark_| is
    // instantiated.
    if (checkmark_)
      checkmark_->SetVisible(selected());

    request()->SetSelectedCreditCard(card_);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (IsComplete()) {
      list()->SelectItem(this);
    } else {
      // TODO(anthonyvd): Display the editor, pre-populated with the data that
      // already exists in |card|.
    }
  }

  bool IsComplete() const {
    // TODO(anthonyvd): Hook this up to the card completedness logic when it's
    // implemented in PaymentRequest.
    return true;
  }

  autofill::CreditCard* card_;
  std::unique_ptr<views::ImageView> checkmark_;

  DISALLOW_COPY_AND_ASSIGN(PaymentMethodListItem);
};

}  // namespace

PaymentMethodViewController::PaymentMethodViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {
  const std::vector<autofill::CreditCard*>& available_cards =
      request->credit_cards();

  for (autofill::CreditCard* card : available_cards) {
    std::unique_ptr<PaymentMethodListItem> item =
        base::MakeUnique<PaymentMethodListItem>(
            card, request, &payment_method_list_,
            card == request->selected_credit_card());
    payment_method_list_.AddItem(std::move(item));
  }
}

PaymentMethodViewController::~PaymentMethodViewController() {}

std::unique_ptr<views::View> PaymentMethodViewController::CreateView() {
  std::unique_ptr<views::View> list_view =
      payment_method_list_.CreateListView();
  list_view->set_id(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  return CreatePaymentView(
      CreateSheetHeaderView(
          true,
          l10n_util::GetStringUTF16(
              IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME),
          this),
      std::move(list_view));
}

std::unique_ptr<views::View>
PaymentMethodViewController::CreateExtraFooterView() {
  std::unique_ptr<views::View> extra_view = base::MakeUnique<views::View>();

  extra_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, kPaymentRequestButtonSpacing));

  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_CREDITCARD_CAPTION));
  button->set_tag(static_cast<int>(
      PaymentMethodViewControllerTags::ADD_CREDIT_CARD_BUTTON));
  button->set_id(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON));
  extra_view->AddChildView(button);

  return extra_view;
}

void PaymentMethodViewController::ButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(
        PaymentMethodViewControllerTags::ADD_CREDIT_CARD_BUTTON):
      dialog()->ShowCreditCardEditor();
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
      break;
  }
}

}  // namespace payments
