// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include <unordered_set>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PaymentRequest::PaymentRequest(
    std::unique_ptr<web::PaymentRequest> web_payment_request,
    autofill::PersonalDataManager* personal_data_manager)
    : web_payment_request_(std::move(web_payment_request)),
      personal_data_manager_(personal_data_manager),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_credit_card_(nullptr),
      selected_shipping_option_(nullptr) {
  PopulateProfileCache();
  PopulateCreditCardCache();
  PopulateShippingOptionCache();
}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::set_payment_details(const web::PaymentDetails& details) {
  DCHECK(web_payment_request_);
  web_payment_request_->details = details;
  PopulateShippingOptionCache();
}

payments::CurrencyFormatter* PaymentRequest::GetOrCreateCurrencyFormatter() {
  DCHECK(web_payment_request_);
  if (!currency_formatter_) {
    currency_formatter_.reset(new payments::CurrencyFormatter(
        base::UTF16ToASCII(web_payment_request_->details.total.amount.currency),
        base::UTF16ToASCII(
            web_payment_request_->details.total.amount.currency_system),
        GetApplicationContext()->GetApplicationLocale()));
  }
  return currency_formatter_.get();
}

void PaymentRequest::PopulateProfileCache() {
  for (const auto* profile : personal_data_manager_->GetProfilesToSuggest()) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profile));
    shipping_profiles_.push_back(profile_cache_.back().get());
    // TODO(crbug.com/602666): Implement deduplication rules for profiles.
    contact_profiles_.push_back(profile_cache_.back().get());
  }

  // TODO(crbug.com/602666): Implement prioritization rules for shipping and
  // contact profiles.

  if (!shipping_profiles_.empty())
    selected_shipping_profile_ = shipping_profiles_[0];
  if (!contact_profiles_.empty())
    selected_contact_profile_ = contact_profiles_[0];
}

void PaymentRequest::PopulateCreditCardCache() {
  DCHECK(web_payment_request_);
  std::unordered_set<base::string16> supported_method_types;
  for (const auto& method_data : web_payment_request_->method_data) {
    for (const auto& supported_method : method_data.supported_methods)
      supported_method_types.insert(supported_method);
  }

  std::vector<autofill::CreditCard*> credit_cards =
      personal_data_manager_->GetCreditCardsToSuggest();

  // TODO(crbug.com/602666): Update the following logic to allow basic card
  // payment. https://w3c.github.io/webpayments-methods-card/
  // new PaymentRequest([{supportedMethods: ['basic-card'],
  //                      data: {supportedNetworks:['visa']}]}], ...);

  for (const auto* credit_card : credit_cards) {
    std::string spec_card_type =
        autofill::data_util::GetPaymentRequestData(credit_card->type())
            .basic_card_payment_type;
    if (supported_method_types.find(base::ASCIIToUTF16(spec_card_type)) !=
        supported_method_types.end()) {
      credit_card_cache_.push_back(
          base::MakeUnique<autofill::CreditCard>(*credit_card));
      credit_cards_.push_back(credit_card_cache_.back().get());
    }
  }

  // TODO(crbug.com/602666): Implement prioritization rules for credit cards.

  if (!credit_cards_.empty())
    selected_credit_card_ = credit_cards_[0];
}

void PaymentRequest::PopulateShippingOptionCache() {
  DCHECK(web_payment_request_);
  shipping_options_.clear();
  shipping_options_.reserve(
      web_payment_request_->details.shipping_options.size());
  std::transform(std::begin(web_payment_request_->details.shipping_options),
                 std::end(web_payment_request_->details.shipping_options),
                 std::back_inserter(shipping_options_),
                 [](web::PaymentShippingOption& option) { return &option; });

  selected_shipping_option_ = nullptr;
  for (auto* shipping_option : shipping_options_) {
    if (shipping_option->selected) {
      // If more than one option has |selected| set, the last one in the
      // sequence should be treated as the selected item.
      selected_shipping_option_ = shipping_option;
    }
  }
}
