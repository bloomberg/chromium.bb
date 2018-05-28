// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_options_provider.h"
#include "third_party/blink/public/platform/modules/payments/payment_request.mojom.h"
#include "url/gurl.h"

namespace payments {

class PaymentInstrument;

// Identifier for the basic card payment method and google-related payment
// methods in the PaymentMethodData.
extern const char kBasicCardMethodName[];
extern const char kGooglePayMethodName[];
extern const char kAndroidPayMethodName[];

// The spec contains all the options that the merchant has specified about this
// Payment Request. It's a (mostly) read-only view, which can be updated in
// certain occasions by the merchant (see API).
class PaymentRequestSpec : public PaymentOptionsProvider {
 public:
  // This enum represents which bit of information was changed to trigger an
  // update roundtrip with the website.
  enum class UpdateReason {
    NONE,
    SHIPPING_OPTION,
    SHIPPING_ADDRESS,
  };

  // Any class call add itself as Observer via AddObserver() and receive
  // notification about spec events.
  class Observer {
   public:
    // Called when the website is notified that the user selected shipping
    // options or a shipping address. This will be followed by a call to
    // OnSpecUpdated or the PaymentRequest being aborted due to a timeout.
    virtual void OnStartUpdating(UpdateReason reason) {}

    // Called when the provided spec has changed.
    virtual void OnSpecUpdated() = 0;

   protected:
    virtual ~Observer() {}
  };

  PaymentRequestSpec(mojom::PaymentOptionsPtr options,
                     mojom::PaymentDetailsPtr details,
                     std::vector<mojom::PaymentMethodDataPtr> method_data,
                     PaymentRequestSpec::Observer* observer,
                     const std::string& app_locale);
  ~PaymentRequestSpec() override;

  // Called when the merchant has new PaymentDetails. Will recompute every spec
  // state that depends on |details|.
  void UpdateWith(mojom::PaymentDetailsPtr details);

  // Recomputes spec based on details.
  void RecomputeSpecForDetails();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // PaymentOptionsProvider:
  bool request_shipping() const override;
  bool request_payer_name() const override;
  bool request_payer_phone() const override;
  bool request_payer_email() const override;
  PaymentShippingType shipping_type() const override;

  bool supports_basic_card() const { return !supported_card_networks_.empty(); }

  const std::vector<std::string>& supported_card_networks() const {
    return supported_card_networks_;
  }
  const std::set<std::string>& supported_card_networks_set() const {
    return supported_card_networks_set_;
  }
  const std::map<std::string, std::set<std::string>>& stringified_method_data()
      const {
    return stringified_method_data_;
  }
  const std::set<autofill::CreditCard::CardType>& supported_card_types_set()
      const {
    return supported_card_types_set_;
  }
  const std::vector<GURL>& url_payment_method_identifiers() const {
    return url_payment_method_identifiers_;
  }
  const std::set<std::string>& payment_method_identifiers_set() const {
    return payment_method_identifiers_set_;
  }
  // Returns whether the |method_name| was specified as supported through the
  // "basic-card" payment method. If false, it means either the |method_name| is
  // not supported at all, or specified directly in supportedMethods.
  bool IsMethodSupportedThroughBasicCard(const std::string& method_name);

  // Uses CurrencyFormatter to format the value of |currency_amount| with the
  // currency symbol for its currency.
  base::string16 GetFormattedCurrencyAmount(
      const mojom::PaymentCurrencyAmountPtr& currency_amount);

  // Uses CurrencyFormatter to get the formatted currency code for
  // |currency_amount|'s currency.
  std::string GetFormattedCurrencyCode(
      const mojom::PaymentCurrencyAmountPtr& currency_amount);

  mojom::PaymentShippingOption* selected_shipping_option() const {
    return selected_shipping_option_;
  }
  // This may contain a non-empty error returned by the merchant. In this case
  // PaymentRequestState::selected_shipping_option_error_profile() will contain
  // the profile related to the error.
  const base::string16& selected_shipping_option_error() const {
    return selected_shipping_option_error_;
  }

  void StartWaitingForUpdateWith(UpdateReason reason);
  bool IsMixedCurrency() const;

  UpdateReason current_update_reason() const { return current_update_reason_; }

  // Returns the total object of this payment request, taking into account the
  // applicable modifier for |selected_instrument| if any.
  const mojom::PaymentItemPtr& GetTotal(
      PaymentInstrument* selected_instrument) const;
  // Returns the display items for this payment request, taking into account the
  // applicable modifier for |selected_instrument| if any.
  std::vector<const mojom::PaymentItemPtr*> GetDisplayItems(
      PaymentInstrument* selected_instrument) const;

  const std::vector<mojom::PaymentShippingOptionPtr>& GetShippingOptions()
      const;

  const mojom::PaymentDetails& details() const { return *details_.get(); }
  const std::vector<mojom::PaymentMethodDataPtr>& method_data() const {
    return method_data_;
  }

 private:
  // Returns the first applicable modifier in the Payment Request for the
  // |selected_instrument|.
  const mojom::PaymentDetailsModifierPtr* GetApplicableModifier(
      PaymentInstrument* selected_instrument) const;

  // Updates the |selected_shipping_option| based on the data passed to this
  // payment request by the website. This will set selected_shipping_option_ to
  // the last option marked selected in the options array. If no options are
  // provided and this method is called |after_update|, it means the merchant
  // doesn't ship to this location. In this case,
  // |selected_shipping_option_error_| will be set.
  void UpdateSelectedShippingOption(bool after_update);

  // Will notify all observers that the spec has changed.
  void NotifyOnSpecUpdated();

  // Returns the CurrencyFormatter instance for this PaymentRequest.
  // |locale_name| should be the result of the browser's GetApplicationLocale().
  // Note: Having multiple currencies per PaymentRequest is not supported; hence
  // the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter(
      const std::string& currency_code,
      const std::string& locale_name);

  mojom::PaymentOptionsPtr options_;
  mojom::PaymentDetailsPtr details_;
  std::vector<mojom::PaymentMethodDataPtr> method_data_;
  const std::string app_locale_;
  // The currently shipping option as specified by the merchant.
  mojom::PaymentShippingOption* selected_shipping_option_;
  base::string16 selected_shipping_option_error_;

  // One currency formatter is instantiated and cached per currency code.
  std::map<std::string, CurrencyFormatter> currency_formatters_;

  // A list/set of supported basic card networks. The list is used to keep the
  // order in which they were specified by the merchant. The set is used for
  // fast lookup of supported methods.
  std::vector<std::string> supported_card_networks_;
  std::set<std::string> supported_card_networks_set_;

  std::set<autofill::CreditCard::CardType> supported_card_types_set_;

  // Only the set of basic-card specified networks. NOTE: callers should use
  // |supported_card_networks_set_| to check merchant support.
  std::set<std::string> basic_card_specified_networks_;

  // A list of supported url-based payment method identifers specified by the
  // merchant. This encompasses one of the two types of payment method
  // identifers, the other being standardized payment method identifiers i.e.,
  // basic-card.
  std::vector<GURL> url_payment_method_identifiers_;

  // The set of all payment method identifiers.
  std::set<std::string> payment_method_identifiers_set_;

  // A mapping of the payment method names to the corresponding JSON-stringified
  // payment method specific data.
  std::map<std::string, std::set<std::string>> stringified_method_data_;

  // The reason why this payment request is waiting for updateWith.
  UpdateReason current_update_reason_;

  // The |observer_for_testing_| will fire after all the |observers_| have been
  // notified.
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSpec);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_
