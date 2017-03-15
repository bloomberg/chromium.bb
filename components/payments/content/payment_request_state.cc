// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/autofill_payment_instrument.h"

namespace payments {

namespace {
// Identifier for the basic card payment method in the PaymentMethodData.
static const char* const kBasicCardMethodName = "basic-card";
}  // namespace

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
      selected_credit_card_(nullptr),
      selected_shipping_option_(nullptr) {
  PopulateProfileCache();
  UpdateSelectedShippingOption();
  SetDefaultProfileSelections();
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

  payment_response->method_name = method_name;
  payment_response->stringified_details = stringified_details;
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::GeneratePaymentResponse() {
  // TODO(mathp): PaymentRequest should know about the currently selected
  // instrument, and not |selected_credit_card_| which is too specific.
  // TODO(mathp): The method_name should reflect what the merchant asked, and
  // not necessarily basic-card.
  selected_payment_instrument_.reset(new AutofillPaymentInstrument(
      kBasicCardMethodName, *selected_credit_card_, shipping_profiles_,
      app_locale_));
  // Fetch the instrument details, will call back into
  // PaymentRequest::OnInstrumentsDetailsReady.
  selected_payment_instrument_->InvokePaymentApp(this);
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

void PaymentRequestState::SetSelectedCreditCard(autofill::CreditCard* card) {
  selected_credit_card_ = card;
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

  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager_->GetCreditCardsToSuggest();
  for (autofill::CreditCard* card : cards) {
    card_cache_.push_back(base::MakeUnique<autofill::CreditCard>(*card));
    credit_cards_.push_back(card_cache_.back().get());
  }
}

void PaymentRequestState::SetDefaultProfileSelections() {
  if (!shipping_profiles().empty())
    selected_shipping_profile_ = shipping_profiles()[0];

  if (!contact_profiles().empty())
    selected_contact_profile_ = contact_profiles()[0];

  // TODO(anthonyvd): Change this code to prioritize server cards and implement
  // a way to modify this function's return value.
  const std::vector<autofill::CreditCard*> cards = credit_cards();
  auto first_complete_card =
      std::find_if(cards.begin(), cards.end(),
                   [](autofill::CreditCard* card) { return card->IsValid(); });

  selected_credit_card_ =
      first_complete_card == cards.end() ? nullptr : *first_complete_card;

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
  // TODO(mathp): A masked card may not satisfy IsValid().
  if (selected_credit_card_ == nullptr || !selected_credit_card_->IsValid())
    return false;

  const std::string basic_card_payment_type =
      autofill::data_util::GetPaymentRequestData(selected_credit_card_->type())
          .basic_card_payment_type;
  return !spec_->supported_card_networks().empty() &&
         std::find(spec_->supported_card_networks().begin(),
                   spec_->supported_card_networks().end(),
                   basic_card_payment_type) !=
             spec_->supported_card_networks().end();
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
