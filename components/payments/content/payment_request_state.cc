// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_request_delegate.h"

namespace payments {

PaymentRequestState::PaymentRequestState(
    PaymentRequestSpec* spec,
    Delegate* delegate,
    const std::string& app_locale,
    autofill::PersonalDataManager* personal_data_manager,
    PaymentRequestDelegate* payment_request_delegate)
    : is_ready_to_pay_(false),
      is_waiting_for_merchant_validation_(false),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      personal_data_manager_(personal_data_manager),
      selected_shipping_profile_(nullptr),
      selected_shipping_option_error_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_instrument_(nullptr),
      payment_request_delegate_(payment_request_delegate),
      profile_comparator_(app_locale, *spec) {
  PopulateProfileCache();
  SetDefaultProfileSelections();
  spec_->AddObserver(this);
}
PaymentRequestState::~PaymentRequestState() {}

void PaymentRequestState::OnPaymentResponseReady(
    mojom::PaymentResponsePtr payment_response) {
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::OnAddressNormalized(
    const autofill::AutofillProfile& normalized_profile) {
  delegate_->OnShippingAddressSelected(
      PaymentResponseHelper::GetMojomPaymentAddressFromAutofillProfile(
          normalized_profile, app_locale_));
}

void PaymentRequestState::OnCouldNotNormalize(
    const autofill::AutofillProfile& profile) {
  // Since the phone number is formatted in either case, this profile should be
  // used.
  OnAddressNormalized(profile);
}

void PaymentRequestState::OnSpecUpdated() {
  if (spec_->selected_shipping_option_error().empty()) {
    selected_shipping_option_error_profile_ = nullptr;
  } else {
    selected_shipping_option_error_profile_ = selected_shipping_profile_;
    selected_shipping_profile_ = nullptr;
  }
  is_waiting_for_merchant_validation_ = false;
  UpdateIsReadyToPayAndNotifyObservers();
}

bool PaymentRequestState::CanMakePayment() const {
  for (const std::unique_ptr<PaymentInstrument>& instrument :
       available_instruments_) {
    if (instrument->IsValidForCanMakePayment()) {
      // AddAutofillPaymentInstrument() filters out available instruments based
      // on supported card networks.
      DCHECK(spec_->supported_card_networks_set().find(
                 instrument->method_name()) !=
             spec_->supported_card_networks_set().end());
      return true;
    }
  }
  return false;
}

bool PaymentRequestState::AreRequestedMethodsSupported() const {
  return !spec_->supported_card_networks().empty();
}

std::string PaymentRequestState::GetAuthenticatedEmail() const {
  return payment_request_delegate_->GetAuthenticatedEmail();
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
      app_locale_, spec_, selected_instrument_, payment_request_delegate_,
      selected_shipping_profile_, selected_contact_profile_, this);
}

void PaymentRequestState::RecordUseStats() {
  if (spec_->request_shipping()) {
    DCHECK(selected_shipping_profile_);
    personal_data_manager_->RecordUseOf(*selected_shipping_profile_);
  }

  if (spec_->request_payer_name() || spec_->request_payer_email() ||
      spec_->request_payer_phone()) {
    DCHECK(selected_contact_profile_);

    // If the same address was used for both contact and shipping, the stats
    // should only be updated once.
    if (!spec_->request_shipping() || (selected_shipping_profile_->guid() !=
                                       selected_contact_profile_->guid())) {
      personal_data_manager_->RecordUseOf(*selected_contact_profile_);
    }
  }

  selected_instrument_->RecordUse();
}

void PaymentRequestState::AddAutofillPaymentInstrument(
    bool selected,
    const autofill::CreditCard& card) {
  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network;
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

void PaymentRequestState::AddAutofillShippingProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      base::MakeUnique<autofill::AutofillProfile>(profile));
  // TODO(tmartino): Implement deduplication rules specific to shipping
  // profiles.
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  shipping_profiles_.push_back(new_cached_profile);

  if (selected)
    SetSelectedShippingProfile(new_cached_profile);
}

void PaymentRequestState::AddAutofillContactProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      base::MakeUnique<autofill::AutofillProfile>(profile));
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  contact_profiles_.push_back(new_cached_profile);

  if (selected)
    SetSelectedContactProfile(new_cached_profile);
}

void PaymentRequestState::SetSelectedShippingOption(
    const std::string& shipping_option_id) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION);
  // This will inform the merchant and will lead to them calling updateWith with
  // new PaymentDetails.
  delegate_->OnShippingOptionIdSelected(shipping_option_id);
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS);
  selected_shipping_profile_ = profile;

  // The user should not be able to click on pay until the callback from the
  // merchant.
  is_waiting_for_merchant_validation_ = true;
  UpdateIsReadyToPayAndNotifyObservers();

  // Start the normalization of the shipping address.
  // Use the country code from the profile if it is set, otherwise infer it
  // from the |app_locale_|.
  std::string country_code = data_util::GetCountryCodeWithFallback(
      selected_shipping_profile_, app_locale_);
  payment_request_delegate_->GetAddressNormalizer()->StartAddressNormalization(
      *selected_shipping_profile_, country_code, /*timeout_seconds=*/2, this);
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

autofill::RegionDataLoader* PaymentRequestState::GetRegionDataLoader() {
  return payment_request_delegate_->GetRegionDataLoader();
}

bool PaymentRequestState::IsPaymentAppInvoked() const {
  return !!response_helper_;
}

void PaymentRequestState::PopulateProfileCache() {
  std::vector<autofill::AutofillProfile*> profiles =
      personal_data_manager_->GetProfilesToSuggest();

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profiles.size());

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profiles[i]));
    raw_profiles_for_filtering.push_back(profile_cache_.back().get());
  }

  contact_profiles_ = profile_comparator()->FilterProfilesForContact(
      raw_profiles_for_filtering);
  shipping_profiles_ = profile_comparator()->FilterProfilesForShipping(
      raw_profiles_for_filtering);

  // Create the list of available instruments. A copy of each card will be made
  // by their respective AutofillPaymentInstrument.
  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager_->GetCreditCardsToSuggest();
  for (autofill::CreditCard* card : cards)
    AddAutofillPaymentInstrument(/*selected=*/false, *card);
}

void PaymentRequestState::SetDefaultProfileSelections() {
  // Only pre-select an address if the merchant provided at least one selected
  // shipping option, and the top profile is complete. Assumes that profiles
  // have already been sorted for completeness and frecency.
  if (!shipping_profiles().empty() && spec_->selected_shipping_option() &&
      profile_comparator()->IsShippingComplete(shipping_profiles_[0])) {
    selected_shipping_profile_ = shipping_profiles()[0];
  }

  // Contact profiles were ordered by completeness in addition to frecency;
  // the first one is the best default selection.
  if (!contact_profiles().empty() &&
      profile_comparator()->IsContactInfoComplete(contact_profiles_[0]))
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
  if (is_waiting_for_merchant_validation_)
    return false;

  if (!profile_comparator()->IsShippingComplete(selected_shipping_profile_))
    return false;

  if (spec_->request_shipping() && !spec_->selected_shipping_option())
    return false;

  return profile_comparator()->IsContactInfoComplete(selected_contact_profile_);
}

}  // namespace payments
