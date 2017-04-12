// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_request_delegate.h"
#include "components/payments/core/profile_util.h"

namespace payments {

PaymentRequestState::PaymentRequestState(
    PaymentRequestSpec* spec,
    Delegate* delegate,
    const std::string& app_locale,
    autofill::PersonalDataManager* personal_data_manager,
    PaymentRequestDelegate* payment_request_delegate)
    : is_ready_to_pay_(false),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      personal_data_manager_(personal_data_manager),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_instrument_(nullptr),
      payment_request_delegate_(payment_request_delegate) {
  PopulateProfileCache();
  SetDefaultProfileSelections();
}
PaymentRequestState::~PaymentRequestState() {}

void PaymentRequestState::OnPaymentResponseReady(
    mojom::PaymentResponsePtr payment_response) {
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

bool PaymentRequestState::CanMakePayment() const {
  for (const std::unique_ptr<PaymentInstrument>& instrument :
       available_instruments_) {
    if (instrument->IsValidForCanMakePayment() &&
        spec_->supported_card_networks_set().count(
            instrument.get()->method_name())) {
      return true;
    }
  }
  return false;
}

void PaymentRequestState::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PaymentRequestState::GeneratePaymentResponse() {
  DCHECK(is_ready_to_pay());

  // Once the response is ready, will call back into OnPaymentResponseReady.
  response_helper_ = base::MakeUnique<PaymentResponseHelper>(
      app_locale_, spec_, selected_instrument_, selected_shipping_profile_,
      selected_contact_profile_, this);
}

void PaymentRequestState::AddAutofillPaymentInstrument(
    bool selected,
    const autofill::CreditCard& card) {
  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(card.type())
          .basic_card_payment_type;
  if (!spec_->supported_card_networks_set().count(basic_card_network))
    return;

  // AutofillPaymentInstrument makes a copy of |card| so it is effectively
  // owned by this object.
  std::unique_ptr<PaymentInstrument> instrument =
      base::MakeUnique<AutofillPaymentInstrument>(
          basic_card_network, card, shipping_profiles_, app_locale_,
          payment_request_delegate_);
  available_instruments_.push_back(std::move(instrument));

  if (selected)
    SetSelectedInstrument(available_instruments_.back().get());
}

void PaymentRequestState::SetSelectedShippingOption(
    const std::string& shipping_option_id) {
  // This will inform the merchant and will lead to them calling updateWith with
  // new PaymentDetails.
  delegate_->OnShippingOptionIdSelected(shipping_option_id);
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile) {
  selected_shipping_profile_ = profile;
  UpdateIsReadyToPayAndNotifyObservers();
  delegate_->OnShippingAddressSelected(
      PaymentResponseHelper::GetMojomPaymentAddressFromAutofillProfile(
          selected_shipping_profile_, app_locale_));
}

void PaymentRequestState::SetSelectedContactProfile(
    autofill::AutofillProfile* profile) {
  selected_contact_profile_ = profile;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::SetSelectedInstrument(PaymentInstrument* instrument) {
  selected_instrument_ = instrument;
  UpdateIsReadyToPayAndNotifyObservers();
}

const std::string& PaymentRequestState::GetApplicationLocale() {
  return app_locale_;
}

autofill::PersonalDataManager* PaymentRequestState::GetPersonalDataManager() {
  return personal_data_manager_;
}

void PaymentRequestState::PopulateProfileCache() {
  std::vector<autofill::AutofillProfile*> profiles =
      personal_data_manager_->GetProfilesToSuggest();

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested. The same is true for credit cards.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profiles[i]));

    // TODO(tmartino): Implement deduplication rules specific to shipping
    // profiles.
    shipping_profiles_.push_back(profile_cache_[i].get());
  }

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering(
      profile_cache_.size());
  std::transform(profile_cache_.begin(), profile_cache_.end(),
                 raw_profiles_for_filtering.begin(),
                 [](const std::unique_ptr<autofill::AutofillProfile>& p) {
                   return p.get();
                 });

  contact_profiles_ = profile_util::FilterProfilesForContact(
      raw_profiles_for_filtering, GetApplicationLocale(), *spec_);

  // Create the list of available instruments.
  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager_->GetCreditCardsToSuggest();
  for (autofill::CreditCard* card : cards)
    AddAutofillPaymentInstrument(/*selected=*/false, *card);
}

void PaymentRequestState::SetDefaultProfileSelections() {
  // Only pre-select an address if the merchant provided at least one selected
  // shipping option.
  if (!shipping_profiles().empty() && spec_->selected_shipping_option())
    selected_shipping_profile_ = shipping_profiles()[0];

  if (!contact_profiles().empty())
    selected_contact_profile_ = contact_profiles()[0];

  // TODO(crbug.com/702063): Change this code to prioritize instruments by use
  // count and other means, and implement a way to modify this function's return
  // value.
  const std::vector<std::unique_ptr<PaymentInstrument>>& instruments =
      available_instruments();
  auto first_complete_instrument =
      std::find_if(instruments.begin(), instruments.end(),
                   [](const std::unique_ptr<PaymentInstrument>& instrument) {
                     return instrument->IsCompleteForPayment();
                   });

  selected_instrument_ = first_complete_instrument == instruments.end()
                             ? nullptr
                             : first_complete_instrument->get();

  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::UpdateIsReadyToPayAndNotifyObservers() {
  is_ready_to_pay_ =
      ArePaymentDetailsSatisfied() && ArePaymentOptionsSatisfied();
  NotifyOnSelectedInformationChanged();
}

void PaymentRequestState::NotifyOnSelectedInformationChanged() {
  for (auto& observer : observers_)
    observer.OnSelectedInformationChanged();
}

bool PaymentRequestState::ArePaymentDetailsSatisfied() {
  // There is no need to check for supported networks, because only supported
  // instruments are listed/created in the flow.
  return selected_instrument_ != nullptr &&
         selected_instrument_->IsCompleteForPayment();
}

bool PaymentRequestState::ArePaymentOptionsSatisfied() {
  // TODO(mathp): Have a measure of shipping address completeness.
  if (spec_->request_shipping() && selected_shipping_profile_ == nullptr)
    return false;

  profile_util::PaymentsProfileComparator comparator(app_locale_, *spec_);
  return comparator.IsContactInfoComplete(selected_contact_profile_);
}

}  // namespace payments
