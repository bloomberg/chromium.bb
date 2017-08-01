// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/address_normalization_manager.h"
#include "components/payments/core/address_normalizer_impl.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payments_profile_comparator.h"
#import "ios/chrome/browser/payments/payment_response_helper.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

namespace payments {
class AddressNormalizer;
class AutofillPaymentInstrument;
class CurrencyFormatter;
}  // namespace payments

namespace ios {
class ChromeBrowserState;
}  // namepsace ios

namespace web {
class WebState;
}  // namespace web

// A protocol implementd by any UI classes that the PaymentRequest object
// needs to communicate with in order to perform certain actions such as
// initiating UI to request full card details for payment.
@protocol PaymentRequestUIDelegate<NSObject>

- (void)
requestFullCreditCard:(const autofill::CreditCard&)creditCard
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               resultDelegate;

- (void)launchAppWithUniversalLink:(std::string)universalLink
                instrumentDelegate:
                    (payments::PaymentInstrument::Delegate*)instrumentDelegate;

@end

namespace payments {

// Has a copy of web::PaymentRequest as provided by the page invoking the
// PaymentRequest API. Also caches credit cards and addresses provided by the
// |personal_data_manager| and manages shared resources and user selections for
// the current PaymentRequest flow. It must be initialized with non-null
// instances of |browser_state|, |web_state|, and |personal_data_manager| that
// outlive this class.
class PaymentRequest : public PaymentOptionsProvider,
                       public PaymentRequestBaseDelegate {
 public:
  // |personal_data_manager| should not be null and should outlive this object.
  PaymentRequest(const web::PaymentRequest& web_payment_request,
                 ios::ChromeBrowserState* browser_state,
                 web::WebState* web_state,
                 autofill::PersonalDataManager* personal_data_manager,
                 id<PaymentRequestUIDelegate> payment_request_ui_delegate);
  ~PaymentRequest() override;

  // Functor used as a simplified comparison function for unique pointers to
  // PaymentRequest. Only compares |web_payment_request_.payment_request_id|.
  struct Compare {
    bool operator()(const std::unique_ptr<PaymentRequest>& lhs,
                    const std::unique_ptr<PaymentRequest>& rhs) const;
  };

  // PaymentRequestBaseDelegate:
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsIncognito() const override;
  bool IsSslCertificateValid() override;
  const GURL& GetLastCommittedURL() const override;
  void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) override;
  AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;

  // Returns the web::PaymentRequest that was used to build this PaymentRequest.
  const web::PaymentRequest& web_payment_request() const {
    return web_payment_request_;
  }

  // Returns the payment details from |web_payment_request_|.
  const web::PaymentDetails& payment_details() const {
    return web_payment_request_.details;
  }

  // Returns the JourneyLogger for this instance.
  JourneyLogger& journey_logger() { return journey_logger_; }

  // Updates the payment details of the |web_payment_request_|. It also updates
  // the cached references to the shipping options in |web_payment_request_| as
  // well as the reference to the selected shipping option.
  void UpdatePaymentDetails(const web::PaymentDetails& details);

  // PaymentOptionsProvider:
  bool request_shipping() const override;
  bool request_payer_name() const override;
  bool request_payer_phone() const override;
  bool request_payer_email() const override;
  PaymentShippingType shipping_type() const override;

  // Returns the payments::CurrencyFormatter instance for this PaymentRequest.
  // Note: Having multiple currencies per PaymentRequest flow is not supported;
  // hence the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter();

  // Returns the AddressNormalizationManager for this instance.
  virtual AddressNormalizationManager* GetAddressNormalizationManager();

  // Adds |profile| to the list of cached profiles, updates the list of
  // available shipping and contact profiles, and returns a reference to the
  // cached copy of |profile|.
  virtual autofill::AutofillProfile* AddAutofillProfile(
      const autofill::AutofillProfile& profile);

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

  const std::vector<std::string>& supported_card_networks() const {
    return supported_card_networks_;
  }

  const std::vector<std::string>& url_payment_method_identifiers() const {
    return url_payment_method_identifiers_;
  }

  const std::map<std::string, std::set<std::string>>& stringified_method_data()
      const {
    return stringified_method_data_;
  }

  const std::set<autofill::CreditCard::CardType>& supported_card_types_set()
      const {
    return supported_card_types_set_;
  }

  // Creates and adds an AutofillPaymentInstrument, which makes a copy of
  // |credit_card|.
  virtual AutofillPaymentInstrument* AddAutofillPaymentInstrument(
      const autofill::CreditCard& credit_card);

  // Returns the available payment methods for this user that match a supported
  // type specified in |web_payment_request_|.
  const std::vector<PaymentInstrument*>& payment_methods() const {
    return payment_methods_;
  }

  // Returns the currently selected payment method for this PaymentRequest flow
  // if there is one. Returns nullptr if there is no selected payment method.
  PaymentInstrument* selected_payment_method() const {
    return selected_payment_method_;
  }

  // Sets the currently selected payment method for this PaymentRequest flow.
  void set_selected_payment_method(PaymentInstrument* payment_method) {
    selected_payment_method_ = payment_method;
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

  virtual PaymentsProfileComparator* profile_comparator();

  // Returns a const version of what the non-const |profile_comparator()|
  // method above returns.
  const PaymentsProfileComparator* profile_comparator() const;

  // Returns whether the current PaymentRequest can be used to make a payment.
  bool CanMakePayment() const;

  // Invokes the appropriate payment app for the selected payment method.
  void InvokePaymentApp(id<PaymentResponseHelperConsumer> consumer);

  // Returns whether the payment app has been invoked.
  bool IsPaymentAppInvoked() const;

  // Record the use of the data models that were used in the Payment Request.
  void RecordUseStats();

 protected:
  // Fetches the autofill profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this PaymentRequest, in profile_cache_.
  void PopulateProfileCache();

  // Sets the available shipping and contact profiles as references to the
  // cached profiles ordered by completeness.
  void PopulateAvailableProfiles();

  // Fetches the payment methods for this user that match a supported type
  // specified in |web_payment_request_| and stores copies of them, owned
  // by this PaymentRequest, in payment_method_cache_.
  void PopulatePaymentMethodCache();

  // Sets the available payment methods as references to the cached payment
  // methods.
  void PopulateAvailablePaymentMethods();

  // Sets the available shipping options as references to the shipping options
  // in |web_payment_request_|.
  void PopulateAvailableShippingOptions();

  // Sets the selected shipping option, if any.
  void SetSelectedShippingOption();

  // The web::PaymentRequest object as provided by the page invoking the Payment
  // Request API, owned by this object.
  web::PaymentRequest web_payment_request_;

  // Never null and outlives this object.
  ios::ChromeBrowserState* browser_state_;

  // Never null and outlives this object.
  web::WebState* web_state_;

  // Never null and outlives this object.
  autofill::PersonalDataManager* personal_data_manager_;

  // The PaymentRequestUIDelegate as provided by the UI object that originally
  // created this PaymentRequest object.
  __weak id<PaymentRequestUIDelegate> payment_request_ui_delegate_;

  // The address normalizer to use for the duration of the Payment Request.
  AddressNormalizerImpl address_normalizer_;

  // Used to normalize the shipping address and the contact info.
  AddressNormalizationManager address_normalization_manager_;

  // The currency formatter instance for this PaymentRequest flow.
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  // Profiles returned by the Data Manager may change due to (e.g.) sync events,
  // meaning PaymentRequest may outlive them. Therefore, profiles are fetched
  // once and their copies are cached here. Whenever profiles are requested a
  // vector of pointers to these copies are returned.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;

  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  autofill::AutofillProfile* selected_shipping_profile_;

  std::vector<autofill::AutofillProfile*> contact_profiles_;
  autofill::AutofillProfile* selected_contact_profile_;

  // Some payment methods, such as credit cards returned by the Data Manager,
  // may change due to (e.g.) sync events, meaning PaymentRequest may outlive
  // them. Therefore, payment methods are fetched once and their copies are
  // cached here. Whenever payment methods are requested a vector of pointers to
  // these copies are returned.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_method_cache_;

  std::vector<PaymentInstrument*> payment_methods_;
  PaymentInstrument* selected_payment_method_;

  // A vector of supported basic card networks.
  std::vector<std::string> supported_card_networks_;
  // A subset of |supported_card_networks_| which is only the networks that have
  // been specified as part of the "basic-card" supported method. Callers should
  // use |supported_card_networks_| for merchant support checks.
  std::set<std::string> basic_card_specified_networks_;

  // A vector of url-based payment method identifiers supported by the merchant
  // which encompasses one of the two types of payment method identifiers, the
  // other being standardized payment method identifiers i.e., basic-card.
  std::vector<std::string> url_payment_method_identifiers_;

  // A mapping of the payment method names to the corresponding JSON-stringified
  // payment method specific data.
  std::map<std::string, std::set<std::string>> stringified_method_data_;

  // The set of supported card types (e.g., credit, debit, prepaid).
  std::set<autofill::CreditCard::CardType> supported_card_types_set_;

  // A vector of pointers to the shipping options in |web_payment_request_|.
  std::vector<web::PaymentShippingOption*> shipping_options_;
  web::PaymentShippingOption* selected_shipping_option_;

  PaymentsProfileComparator profile_comparator_;

  // Keeps track of different stats during the lifetime of this object.
  JourneyLogger journey_logger_;

  std::unique_ptr<PaymentResponseHelper> response_helper_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
