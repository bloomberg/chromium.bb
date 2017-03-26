// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <set>

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/autofill_payment_instrument.h"

namespace payments {

PaymentRequestState::PaymentRequestState(
    PaymentRequestSpec* spec,
    Delegate* delegate,
    const std::string& app_locale,
    autofill::PersonalDataManager* personal_data_manager)
    : is_ready_to_pay_(false),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      personal_data_manager_(personal_data_manager),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_instrument_(nullptr),
      selected_shipping_option_(nullptr) {
  PopulateProfileCache();
  UpdateSelectedShippingOption();
  SetDefaultProfileSelections();
}

bool PaymentRequestState::CanMakePayment() const {
  for (const std::unique_ptr<PaymentInstrument>& instrument :
       available_instruments_) {
    if (instrument.get()->IsValid() &&
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
PaymentRequestState::~PaymentRequestState() {}

void PaymentRequestState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PaymentRequestState::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details) {
  // TODO(mathp): Fill other fields in the PaymentResponsePtr object.
  mojom::PaymentResponsePtr payment_response = mojom::PaymentResponse::New();

  // Make sure that we return the method name that the merchant specified for
  // this instrument: cards can be either specified through their name (e.g.,
  // "visa") or through basic-card's supportedNetworks.
  payment_response->method_name =
      spec_->IsMethodSupportedThroughBasicCard(method_name)
          ? kBasicCardMethodName
          : method_name;
  payment_response->stringified_details = stringified_details;
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::GeneratePaymentResponse() {
  DCHECK(is_ready_to_pay());
  // Fetch the instrument details, will call back into
  // PaymentRequest::OnInstrumentsDetailsReady.
  selected_instrument_->InvokePaymentApp(this);
}

void PaymentRequestState::SetSelectedShippingOption(
    mojom::PaymentShippingOption* option) {
  selected_shipping_option_ = option;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile) {
  selected_shipping_profile_ = profile;
  UpdateIsReadyToPayAndNotifyObservers();
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

    // TODO(tmartino): Implement deduplication rules specific to shipping and
    // contact profiles.
    shipping_profiles_.push_back(profile_cache_[i].get());
    contact_profiles_.push_back(profile_cache_[i].get());
  }

  // Create the list of available instruments.
  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager_->GetCreditCardsToSuggest();
  const std::set<std::string>& supported_card_networks =
      spec_->supported_card_networks_set();
  for (autofill::CreditCard* card : cards) {
    std::string basic_card_network =
        autofill::data_util::GetPaymentRequestData(card->type())
            .basic_card_payment_type;
    if (!supported_card_networks.count(basic_card_network))
      continue;

    // TODO(crbug.com/701952): Should use the method name preferred by the
    // merchant (either "basic-card" or the basic card network e.g. "visa").

    // Copy the credit cards as part of AutofillPaymentInstrument so they are
    // indirectly owned by this object.
    std::unique_ptr<PaymentInstrument> instrument =
        base::MakeUnique<AutofillPaymentInstrument>(
            basic_card_network, *card, shipping_profiles_, app_locale_);
    available_instruments_.push_back(std::move(instrument));
  }
}

void PaymentRequestState::SetDefaultProfileSelections() {
  if (!shipping_profiles().empty())
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
                     return instrument->IsValid();
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
  // TODO(crbug.com/702063): A masked card may not satisfy IsValid().
  return selected_instrument_ != nullptr && selected_instrument_->IsValid();
}

bool PaymentRequestState::ArePaymentOptionsSatisfied() {
  // TODO(mathp): Have a measure of shipping address completeness.
  if (spec_->request_shipping() && selected_shipping_profile_ == nullptr)
    return false;

  // TODO(mathp): Make an encompassing class to validate contact info.
  if (spec_->request_payer_name() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::NAME_FULL), app_locale_)
           .empty())) {
    return false;
  }
  if (spec_->request_payer_email() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                     app_locale_)
           .empty())) {
    return false;
  }
  if (spec_->request_payer_phone() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                     app_locale_)
           .empty())) {
    return false;
  }

  return true;
}

void PaymentRequestState::UpdateSelectedShippingOption() {
  selected_shipping_option_ = nullptr;

  // As per the spec, the selected shipping option should initially be the last
  // one in the array that has its selected field set to true.
  auto selected_shipping_option_it = std::find_if(
      spec_->details().shipping_options.rbegin(),
      spec_->details().shipping_options.rend(),
      [](const payments::mojom::PaymentShippingOptionPtr& element) {
        return element->selected;
      });
  if (selected_shipping_option_it != spec_->details().shipping_options.rend()) {
    selected_shipping_option_ = selected_shipping_option_it->get();
  }
}

}  // namespace payments
