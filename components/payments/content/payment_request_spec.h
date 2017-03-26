// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/payments/content/payment_request.mojom.h"
#include "components/payments/core/currency_formatter.h"

namespace payments {

// Identifier for the basic card payment method in the PaymentMethodData.
extern const char kBasicCardMethodName[];

// The spec contains all the options that the merchant has specified about this
// Payment Request. It's a (mostly) read-only view, which can be updated in
// certain occasions by the merchant (see API).
class PaymentRequestSpec {
 public:
  // Any class call add itself as Observer via AddObserver() and receive
  // notification about spec events.
  class Observer {
   public:
    // Called when the provided spec (details, options, method_data) is invalid.
    virtual void OnInvalidSpecProvided() = 0;

   protected:
    virtual ~Observer() {}
  };

  PaymentRequestSpec(mojom::PaymentOptionsPtr options,
                     mojom::PaymentDetailsPtr details,
                     std::vector<mojom::PaymentMethodDataPtr> method_data,
                     PaymentRequestSpec::Observer* observer,
                     const std::string& app_locale);
  ~PaymentRequestSpec();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool request_shipping() const { return options_->request_shipping; }
  bool request_payer_name() const { return options_->request_payer_name; }
  bool request_payer_phone() const { return options_->request_payer_phone; }
  bool request_payer_email() const { return options_->request_payer_email; }

  const std::vector<std::string>& supported_card_networks() const {
    return supported_card_networks_;
  }
  const std::set<std::string>& supported_card_networks_set() const {
    return supported_card_networks_set_;
  }
  // Returns whether the |method_name| was specified as supported through the
  // "basic-card" payment method. If false, it means either the |method_name| is
  // not supported at all, or specified directly in supportedMethods.
  bool IsMethodSupportedThroughBasicCard(const std::string& method_name);

  // Uses CurrencyFormatter to format |amount| with the currency symbol for this
  // request's currency. Will use currency of the "total" display item, because
  // all items are supposed to have the same currency in a given request.
  base::string16 GetFormattedCurrencyAmount(const std::string& amount);

  // Uses CurrencyFormatter to get the formatted currency code for this
  // request's currency. Will use currency of the "total" display item, because
  // all items are supposed to have the same currency in a given request.
  std::string GetFormattedCurrencyCode();

  const mojom::PaymentDetails& details() const { return *details_.get(); }
  const mojom::PaymentOptions& options() const { return *options_.get(); }

 private:
  // Validates the |method_data| and fills |supported_card_networks_|.
  void PopulateValidatedMethodData(
      const std::vector<mojom::PaymentMethodDataPtr>& method_data);

  // Will notify all observers that the spec is invalid.
  void NotifyOnInvalidSpecProvided();

  // Returns the CurrencyFormatter instance for this PaymentRequest.
  // |locale_name| should be the result of the browser's GetApplicationLocale().
  // Note: Having multiple currencies per PaymentRequest is not supported; hence
  // the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter(
      const std::string& currency_code,
      const std::string& currency_system,
      const std::string& locale_name);

  mojom::PaymentOptionsPtr options_;
  mojom::PaymentDetailsPtr details_;
  const std::string app_locale_;
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  // A list/set of supported basic card networks. The list is used to keep the
  // order in which they were specified by the merchant. The set is used for
  // fast lookup of supported methods.
  std::vector<std::string> supported_card_networks_;
  std::set<std::string> supported_card_networks_set_;

  // Only the set of basic-card specified networks. NOTE: callers should use
  // |supported_card_networks_set_| to check merchant support.
  std::set<std::string> basic_card_specified_networks_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSpec);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_SPEC_H_
