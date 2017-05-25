// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_data_loader_impl.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "ios/web/public/payments/payment_request.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<::i18n::addressinput::Source> GetAddressInputSource(
    net::URLRequestContextGetter* url_context_getter) {
  return std::unique_ptr<::i18n::addressinput::Source>(
      new autofill::ChromeMetadataSource(I18N_ADDRESS_VALIDATION_DATA_URL,
                                         url_context_getter));
}

std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage() {
  return autofill::ValidationRulesStorageFactory::CreateStorage();
}

}  // namespace

PaymentRequest::PaymentRequest(
    const web::PaymentRequest& web_payment_request,
    autofill::PersonalDataManager* personal_data_manager)
    : web_payment_request_(web_payment_request),
      personal_data_manager_(personal_data_manager),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_credit_card_(nullptr),
      selected_shipping_option_(nullptr),
      profile_comparator_(GetApplicationContext()->GetApplicationLocale(),
                          *this) {
  PopulateAvailableShippingOptions();
  PopulateProfileCache();
  PopulateAvailableProfiles();
  PopulateCreditCardCache();
  PopulateAvailableCreditCards();

  SetSelectedShippingOption();

  // If the merchant provided a default shipping option, and the highest-ranking
  // shipping profile is usable, select it.
  if (selected_shipping_option_ && !shipping_profiles_.empty() &&
      profile_comparator_.IsShippingComplete(shipping_profiles_[0])) {
    selected_shipping_profile_ = shipping_profiles_[0];
  }

  // If the highest-ranking contact profile is usable, select it. Otherwise,
  // select none.
  if (!contact_profiles_.empty() &&
      profile_comparator_.IsContactInfoComplete(contact_profiles_[0])) {
    selected_contact_profile_ = contact_profiles_[0];
  }

  // Select the highest ranking credit card.
  if (!credit_cards_.empty())
    selected_credit_card_ = credit_cards_[0];
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::UpdatePaymentDetails(const web::PaymentDetails& details) {
  web_payment_request_.details = details;
  PopulateAvailableShippingOptions();
  SetSelectedShippingOption();
}

bool PaymentRequest::request_shipping() const {
  return web_payment_request_.options.request_shipping;
}

bool PaymentRequest::request_payer_name() const {
  return web_payment_request_.options.request_payer_name;
}

bool PaymentRequest::request_payer_phone() const {
  return web_payment_request_.options.request_payer_phone;
}

bool PaymentRequest::request_payer_email() const {
  return web_payment_request_.options.request_payer_email;
}

payments::PaymentShippingType PaymentRequest::shipping_type() const {
  return web_payment_request_.options.shipping_type;
}

payments::CurrencyFormatter* PaymentRequest::GetOrCreateCurrencyFormatter() {
  if (!currency_formatter_) {
    currency_formatter_.reset(new payments::CurrencyFormatter(
        base::UTF16ToASCII(web_payment_request_.details.total.amount.currency),
        base::UTF16ToASCII(
            web_payment_request_.details.total.amount.currency_system),
        GetApplicationContext()->GetApplicationLocale()));
  }
  return currency_formatter_.get();
}

autofill::RegionDataLoader* PaymentRequest::GetRegionDataLoader() {
  return new autofill::RegionDataLoaderImpl(
      GetAddressInputSource(
          personal_data_manager_->GetURLRequestContextGetter())
          .release(),
      GetAddressInputStorage().release(),
      GetApplicationContext()->GetApplicationLocale());
}

autofill::AutofillProfile* PaymentRequest::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      base::MakeUnique<autofill::AutofillProfile>(profile));

  PopulateAvailableProfiles();

  return profile_cache_.back().get();
}

void PaymentRequest::PopulateProfileCache() {
  const std::vector<autofill::AutofillProfile*>& profiles_to_suggest =
      personal_data_manager_->GetProfilesToSuggest();
  // Return early if the user has no stored Autofill profiles.
  if (profiles_to_suggest.empty())
    return;

  profile_cache_.reserve(profiles_to_suggest.size());

  for (const auto* profile : profiles_to_suggest) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profile));
  }
}

void PaymentRequest::PopulateAvailableProfiles() {
  if (profile_cache_.empty())
    return;

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profile_cache_.size());

  for (auto const& profile : profile_cache_) {
    raw_profiles_for_filtering.push_back(profile.get());
  }

  // Contact profiles are deduped and ordered by completeness.
  contact_profiles_ =
      profile_comparator_.FilterProfilesForContact(raw_profiles_for_filtering);

  // Shipping profiles are ordered by completeness.
  shipping_profiles_ =
      profile_comparator_.FilterProfilesForShipping(raw_profiles_for_filtering);
}

autofill::CreditCard* PaymentRequest::AddCreditCard(
    const autofill::CreditCard& credit_card) {
  credit_card_cache_.push_back(
      base::MakeUnique<autofill::CreditCard>(credit_card));

  PopulateAvailableCreditCards();

  return credit_card_cache_.back().get();
}

bool PaymentRequest::CanMakePayment() const {
  return !credit_cards_.empty();
}

void PaymentRequest::PopulateCreditCardCache() {
  // TODO(crbug.com/709036): Validate method data.
  payments::data_util::ParseBasicCardSupportedNetworks(
      web_payment_request_.method_data, &supported_card_networks_,
      &basic_card_specified_networks_);

  const std::vector<autofill::CreditCard*>& credit_cards_to_suggest =
      personal_data_manager_->GetCreditCardsToSuggest();
  // Return early if the user has no stored credit cards.
  if (credit_cards_to_suggest.empty())
    return;

  credit_card_cache_.reserve(credit_cards_to_suggest.size());

  for (const auto* credit_card : credit_cards_to_suggest) {
    std::string spec_issuer_network =
        autofill::data_util::GetPaymentRequestData(credit_card->network())
            .basic_card_issuer_network;
    if (std::find(supported_card_networks_.begin(),
                  supported_card_networks_.end(),
                  spec_issuer_network) != supported_card_networks_.end()) {
      credit_card_cache_.push_back(
          base::MakeUnique<autofill::CreditCard>(*credit_card));
    }
  }
}

void PaymentRequest::PopulateAvailableCreditCards() {
  if (credit_card_cache_.empty())
    return;

  credit_cards_.clear();
  credit_cards_.reserve(credit_card_cache_.size());

  // TODO(crbug.com/602666): Implement prioritization rules for credit cards.
  for (auto const& credit_card : credit_card_cache_) {
    credit_cards_.push_back(credit_card.get());
  }
}

void PaymentRequest::PopulateAvailableShippingOptions() {
  shipping_options_.clear();
  selected_shipping_option_ = nullptr;
  if (web_payment_request_.details.shipping_options.empty())
    return;

  shipping_options_.reserve(
      web_payment_request_.details.shipping_options.size());
  std::transform(std::begin(web_payment_request_.details.shipping_options),
                 std::end(web_payment_request_.details.shipping_options),
                 std::back_inserter(shipping_options_),
                 [](web::PaymentShippingOption& option) { return &option; });
}

void PaymentRequest::SetSelectedShippingOption() {
  // If more than one option has |selected| set, the last one in the sequence
  // should be treated as the selected item.
  for (auto* shipping_option : base::Reversed(shipping_options_)) {
    if (shipping_option->selected) {
      selected_shipping_option_ = shipping_option;
      break;
    }
  }
}
