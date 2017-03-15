// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

namespace {

class ProfileItem : public PaymentRequestItemList::Item {
 public:
  // Constructs an object owned by |parent_list|, representing one element in
  // the list. |spec| and |state| are the PaymentRequestSpec/State objects that
  // are represented by the current instance of the dialog. |parent_view| points
  // to the controller which owns |parent_list|. |profile| is the
  // AutofillProfile that this specific list item represents. It's a cached
  // profile owned by |state|.
  ProfileItem(autofill::AutofillProfile* profile,
              PaymentRequestSpec* spec,
              PaymentRequestState* state,
              PaymentRequestItemList* parent_list,
              ProfileListViewController* parent_view,
              bool selected)
      : payments::PaymentRequestItemList::Item(spec,
                                               state,
                                               parent_list,
                                               selected),
        parent_view_(parent_view),
        profile_(profile) {}
  ~ProfileItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateItemView() override {
    DCHECK(profile_);

    std::unique_ptr<views::View> content = parent_view_->GetLabel(profile_);

    std::unique_ptr<PaymentRequestRowView> row =
        base::MakeUnique<PaymentRequestRowView>(this);
    views::GridLayout* layout = new views::GridLayout(row.get());
    row->SetLayoutManager(layout);

    layout->SetInsets(
        kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
        kPaymentRequestRowVerticalInsets,
        kPaymentRequestRowHorizontalInsets + kPaymentRequestRowExtraRightInset);

    // Add a column listing the profile information.
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1,
                       views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(1, 0);

    // Add a column for the checkmark shown next to the selected profile.
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

  ProfileListViewController* parent_view_;
  autofill::AutofillProfile* profile_;
  std::unique_ptr<views::ImageView> checkmark_;

  DISALLOW_COPY_AND_ASSIGN(ProfileItem);
};

// The ProfileListViewController subtype for the Shipping address list
// screen of the Payment Request flow.
class ShippingProfileViewController : public ProfileListViewController {
 public:
  ShippingProfileViewController(PaymentRequestSpec* spec,
                                PaymentRequestState* state,
                                PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {}
  ~ShippingProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile) override {
    return GetShippingAddressLabel(AddressStyleType::DETAILED,
                                   state()->GetApplicationLocale(), *profile);
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->shipping_profiles();
  }

  base::string16 GetHeaderString() override {
    return GetShippingAddressSectionString(spec()->options().shipping_type);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShippingProfileViewController);
};

class ContactProfileViewController : public ProfileListViewController {
 public:
  ContactProfileViewController(PaymentRequestSpec* spec,
                               PaymentRequestState* state,
                               PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {}
  ~ContactProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile) override {
    return GetContactInfoLabel(
        AddressStyleType::DETAILED, state()->GetApplicationLocale(), *profile,
        spec()->request_payer_name(), spec()->request_payer_phone(),
        spec()->request_payer_email());
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->contact_profiles();
  }

  base::string16 GetHeaderString() override {
    return l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactProfileViewController);
};

}  // namespace

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetShippingProfileViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog) {
  return base::MakeUnique<ShippingProfileViewController>(spec, state, dialog);
}

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetContactProfileViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog) {
  return base::MakeUnique<ContactProfileViewController>(spec, state, dialog);
}

ProfileListViewController::ProfileListViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog) {}

ProfileListViewController::~ProfileListViewController() {}

std::unique_ptr<views::View> ProfileListViewController::CreateView() {
  autofill::AutofillProfile* selected_profile =
      state()->selected_shipping_profile();

  // This must be done at Create-time, rather than construct-time, because
  // the subclass method GetProfiles can't be called in the ctor.
  for (auto* profile : GetProfiles()) {
    list_.AddItem(base::MakeUnique<ProfileItem>(
        profile, spec(), state(), &list_, this, profile == selected_profile));
  }

  return CreatePaymentView(
      CreateSheetHeaderView(
          /* show_back_arrow = */ true, GetHeaderString(), this),
      list_.CreateListView());
}

}  // namespace payments
