// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

namespace payments {
class CurrencyFormatter;
}  // namespace payments

// Owns an instance of web::PaymentRequest as provided by the page invoking the
// PaymentRequest API. Also caches credit cards and addresses provided by the
// |personal_data_manager| and manages shared resources and user selections for
// the current PaymentRequest flow. It takes ownership of |web_payment_request|
// and must be initialized with a non-null instance of |personal_data_manager|
// that outlives this class.
class PaymentRequest {
 public:
  // |personal_data_manager| should not be null and should outlive this object.
  PaymentRequest(std::unique_ptr<web::PaymentRequest> web_payment_request,
                 autofill::PersonalDataManager* personal_data_manager);
  ~PaymentRequest();

  // Returns the payment details from |web_payment_request_|.
  const web::PaymentDetails& payment_details() const {
    return web_payment_request_->details;
  }

  // Returns the payment options from |web_payment_request_|.
  const web::PaymentOptions& payment_options() const {
    return web_payment_request_->options;
  }

  // Updates the payment details of the |web_payment_request_|. It also updates
  // the cached references to the shipping options in |web_payment_request_| as
  // well as the reference to the selected shipping option.
  void set_payment_details(const web::PaymentDetails& details);

  // Returns the payments::CurrencyFormatter instance for this PaymentRequest.
  // Note: Having multiple currencies per PaymentRequest flow is not supported;
  // hence the CurrencyFormatter is cached here.
  payments::CurrencyFormatter* GetOrCreateCurrencyFormatter();

  // Returns the available autofill profiles for this user to be used as
  // shipping profiles.
  const std::vector<autofill::AutofillProfile*>& shipping_profiles() const {
    return shipping_profiles_;
  }

  // Returns the currently selected shipping profile for this PaymentRequest
  // flow if there is one. Returns nullptr if there is no selected profile.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }

  // Sets the currently selected shipping profile for this PaymentRequest flow.
  void set_selected_shipping_profile(autofill::AutofillProfile* profile) {
    selected_shipping_profile_ = profile;
  }

  // Returns the available autofill profiles for this user to be used as
  // contact profiles.
  const std::vector<autofill::AutofillProfile*>& contact_profiles() const {
    return contact_profiles_;
  }

  // Returns the currently selected contact profile for this PaymentRequest
  // flow if there is one. Returns nullptr if there is no selected profile.
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }

  // Sets the currently selected contact profile for this PaymentRequest flow.
  void set_selected_contact_profile(autofill::AutofillProfile* profile) {
    selected_contact_profile_ = profile;
  }

  // Returns the available autofill profiles for this user to be used as
  // billing profiles.
  const std::vector<autofill::AutofillProfile*>& billing_profiles() const {
    return shipping_profiles_;
  }

  // Returns the available autofill credit cards for this user that match a
  // supported type specified in |web_payment_request_|.
  const std::vector<autofill::CreditCard*>& credit_cards() const {
    return credit_cards_;
  }

  // Returns the currently selected credit card for this PaymentRequest flow if
  // there is one. Returns nullptr if there is no selected credit card.
  autofill::CreditCard* selected_credit_card() const {
    return selected_credit_card_;
  }

  // Sets the currently selected credit card for this PaymentRequest flow.
  void set_selected_credit_card(autofill::CreditCard* credit_card) {
    selected_credit_card_ = credit_card;
  }

  // Returns the available shipping options from |web_payment_request_|.
  const std::vector<web::PaymentShippingOption*>& shipping_options() const {
    return shipping_options_;
  }

  // Returns the selected shipping option from |web_payment_request_| if there
  // is one. Returns nullptr otherwise.
  web::PaymentShippingOption* selected_shipping_option() const {
    return selected_shipping_option_;
  }

 private:
  // Fetches the autofill profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this PaymentRequest, in profile_cache_.
  void PopulateProfileCache();

  // Fetches the autofill credit cards for this user from the
  // PersonalDataManager that match a supported type specified in
  // |web_payment_request_| and stores copies of them, owned by this
  // PaymentRequest, in credit_card_cache_.
  void PopulateCreditCardCache();

  // Stores references to the shipping options in |web_payment_request_|.
  void PopulateShippingOptionCache();

  // The web::PaymentRequest object as provided by the page invoking the Payment
  // Request API, owned by this PaymentRequest.
  std::unique_ptr<web::PaymentRequest> web_payment_request_;

  // Never null and outlives this object.
  autofill::PersonalDataManager* personal_data_manager_;

  // The currency formatter instance for this PaymentRequest flow.
  std::unique_ptr<payments::CurrencyFormatter> currency_formatter_;

  // Profiles returned by the Data Manager may change due to (e.g.) sync events,
  // meaning PaymentRequest may outlive them. Therefore, profiles are fetched
  // once and owned here. Whenever profiles are requested a vector of pointers
  // to these copies are returned.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;

  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  autofill::AutofillProfile* selected_shipping_profile_;

  std::vector<autofill::AutofillProfile*> contact_profiles_;
  autofill::AutofillProfile* selected_contact_profile_;

  // Credit cards returnd by the Data Manager may change due to (e.g.)
  // sync events, meaning PaymentRequest may outlive them. Therefore, credit
  // cards are fetched once and owned here. Whenever credit cards are requested
  // a vector of pointers to these copies are returned.
  std::vector<std::unique_ptr<autofill::CreditCard>> credit_card_cache_;

  std::vector<autofill::CreditCard*> credit_cards_;
  autofill::CreditCard* selected_credit_card_;

  // A vector of pointers to the shipping options in |web_payment_request_|.
  std::vector<web::PaymentShippingOption*> shipping_options_;
  web::PaymentShippingOption* selected_shipping_option_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
