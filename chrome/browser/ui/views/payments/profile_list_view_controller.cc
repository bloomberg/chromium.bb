// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

namespace {

constexpr int kFirstTagValue = static_cast<int>(
    payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX);

enum class PaymentMethodViewControllerTags : int {
  // The tag for the button that triggers the "add address" flow. Starts at
  // |kFirstTagValue| not to conflict with tags common to all views.
  ADD_SHIPPING_ADDRESS_BUTTON = kFirstTagValue,
  ADD_CONTACT_BUTTON,
};

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
              PaymentRequestDialogView* dialog,
              bool selected)
      : payments::PaymentRequestItemList::Item(spec,
                                               state,
                                               parent_list,
                                               selected),
        parent_view_(parent_view),
        profile_(profile),
        dialog_(dialog) {}
  ~ProfileItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateContentView() override {
    DCHECK(profile_);

    return parent_view_->GetLabel(profile_);
  }

  void SelectedStateChanged() override {
    if (selected()) {
      parent_view_->SelectProfile(profile_);
      dialog_->GoBack();
    }
  }

  bool CanBeSelected() const override {
    // TODO(crbug.com/709454): Check for profile completeness. Currently a giant
    // hack to test the UI.
    return !profile_->GetRawInfo(autofill::ADDRESS_HOME_ZIP).empty();
  }

  void PerformSelectionFallback() override {
    parent_view_->ShowEditor(profile_);
  }

  ProfileListViewController* parent_view_;
  autofill::AutofillProfile* profile_;
  PaymentRequestDialogView* dialog_;

  DISALLOW_COPY_AND_ASSIGN(ProfileItem);
};

// The ProfileListViewController subtype for the Shipping address list
// screen of the Payment Request flow.
class ShippingProfileViewController : public ProfileListViewController {
 public:
  ShippingProfileViewController(PaymentRequestSpec* spec,
                                PaymentRequestState* state,
                                PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {
    PopulateList();
  }
  ~ShippingProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile) override {
    return GetShippingAddressLabel(AddressStyleType::DETAILED,
                                   state()->GetApplicationLocale(), *profile);
  }

  void SelectProfile(autofill::AutofillProfile* profile) override {
    state()->SetSelectedShippingProfile(profile);
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowShippingAddressEditor(profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_shipping_profile();
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->shipping_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW;
  }

  base::string16 GetSheetTitle() override {
    return GetShippingAddressSectionString(spec()->shipping_type());
  }

  int GetSecondaryButtonTextId() override {
    return IDS_AUTOFILL_ADD_ADDRESS_CAPTION;
  }

  int GetSecondaryButtonTag() override {
    return static_cast<int>(
        PaymentMethodViewControllerTags::ADD_SHIPPING_ADDRESS_BUTTON);
  }

  int GetSecondaryButtonViewId() override {
    return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_SHIPPING_BUTTON);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShippingProfileViewController);
};

class ContactProfileViewController : public ProfileListViewController {
 public:
  ContactProfileViewController(PaymentRequestSpec* spec,
                               PaymentRequestState* state,
                               PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {
    PopulateList();
  }
  ~ContactProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile) override {
    return GetContactInfoLabel(AddressStyleType::DETAILED,
                               state()->GetApplicationLocale(), *profile,
                               *spec());
  }

  void SelectProfile(autofill::AutofillProfile* profile) override {
    state()->SetSelectedContactProfile(profile);
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowContactInfoEditor(profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_contact_profile();
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->contact_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW;
  }

  base::string16 GetSheetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  }

  int GetSecondaryButtonTextId() override {
    return IDS_AUTOFILL_ADD_CONTACT_CAPTION;
  }

  int GetSecondaryButtonTag() override {
    return static_cast<int>(
        PaymentMethodViewControllerTags::ADD_CONTACT_BUTTON);
  }

  int GetSecondaryButtonViewId() override {
    return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CONTACT_BUTTON);
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

void ProfileListViewController::PopulateList() {
  autofill::AutofillProfile* selected_profile = GetSelectedProfile();

  // This must be done at Create-time, rather than construct-time, because
  // the subclass method GetProfiles can't be called in the ctor.
  for (auto* profile : GetProfiles()) {
    list_.AddItem(base::MakeUnique<ProfileItem>(profile, spec(), state(),
                                                &list_, this, dialog(),
                                                profile == selected_profile));
  }
}

void ProfileListViewController::FillContentView(views::View* content_view) {
  content_view->SetLayoutManager(new views::FillLayout);
  std::unique_ptr<views::View> list_view = list_.CreateListView();
  list_view->set_id(static_cast<int>(GetDialogViewId()));
  content_view->AddChildView(list_view.release());
}

std::unique_ptr<views::View>
ProfileListViewController::CreateExtraFooterView() {
  std::unique_ptr<views::View> extra_view = base::MakeUnique<views::View>();

  extra_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kPaymentRequestButtonSpacing));

  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(GetSecondaryButtonTextId()));
  button->set_tag(GetSecondaryButtonTag());
  button->set_id(GetSecondaryButtonViewId());
  extra_view->AddChildView(button);

  return extra_view;
}

void ProfileListViewController::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender->tag() == GetSecondaryButtonTag())
    ShowEditor(nullptr);
  else
    PaymentRequestSheetController::ButtonPressed(sender, event);
}

}  // namespace payments
