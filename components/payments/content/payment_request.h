// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/payments/content/payment_request.mojom.h"
#include "components/payments/content/payment_request_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
}

namespace content {
class WebContents;
}

namespace payments {

class CurrencyFormatter;
class PaymentRequestWebContentsManager;

class PaymentRequest : payments::mojom::PaymentRequest {
 public:
  class Observer {
   public:
    // Called when the information (payment method, address/contact info,
    // shipping option) changes.
    virtual void OnSelectedInformationChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  PaymentRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PaymentRequestDelegate> delegate,
      PaymentRequestWebContentsManager* manager,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);
  ~PaymentRequest() override;

  // payments::mojom::PaymentRequest "stub"
  void Init(payments::mojom::PaymentRequestClientPtr client,
            std::vector<payments::mojom::PaymentMethodDataPtr> method_data,
            payments::mojom::PaymentDetailsPtr details,
            payments::mojom::PaymentOptionsPtr options) override;
  void Show() override;
  void UpdateWith(payments::mojom::PaymentDetailsPtr details) override {}
  void Abort() override;
  void Complete(payments::mojom::PaymentComplete result) override {}
  void CanMakePayment() override {}

  // Called when the user explicitely cancelled the flow. Will send a message
  // to the renderer which will indirectly destroy this object (through
  // OnConnectionTerminated).
  void UserCancelled();

  // As a result of a browser-side error or renderer-initiated mojo channel
  // closure (e.g. there was an error on the renderer side, or payment was
  // successful), this method is called. It is responsible for cleaning up,
  // such as possibly closing the dialog.
  void OnConnectionTerminated();

  // Called when the user clicks on the "Pay" button.
  void Pay();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the CurrencyFormatter instance for this PaymentRequest.
  // |locale_name| should be the result of the browser's GetApplicationLocale().
  // Note: Having multiple currencies per PaymentRequest is not supported; hence
  // the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter(
      const std::string& currency_code,
      const std::string& currency_system,
      const std::string& locale_name);

  // Returns the appropriate Autofill Profiles for this user. On the first
  // invocation of either getter, the profiles are fetched from the
  // PersonalDataManager; on subsequent invocations, a cached version is
  // returned. The profiles returned are owned by the request object.
  const std::vector<autofill::AutofillProfile*>& shipping_profiles() {
    return shipping_profiles_;
  }
  const std::vector<autofill::AutofillProfile*>& contact_profiles() {
    return contact_profiles_;
  }
  const std::vector<autofill::CreditCard*>& credit_cards() {
    return credit_cards_;
  }

  // Gets the Autofill Profile representing the shipping address or contact
  // information currently selected for this PaymentRequest flow. Can return
  // null.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }
  // Returns the currently selected credit card for this PaymentRequest flow.
  // It's not guaranteed to be complete. Returns nullptr if there is no selected
  // card.
  autofill::CreditCard* selected_credit_card() { return selected_credit_card_; }

  // Sets the |profile| to be the selected one and will update state and notify
  // observers.
  void SetSelectedShippingProfile(autofill::AutofillProfile* profile);
  void SetSelectedContactProfile(autofill::AutofillProfile* profile);
  void SetSelectedCreditCard(autofill::CreditCard* card);

  autofill::PersonalDataManager* personal_data_manager() {
    return delegate_->GetPersonalDataManager();
  }

  payments::mojom::PaymentDetails* details() { return details_.get(); }
  const std::vector<std::string>& supported_card_networks() {
    return supported_card_networks_;
  }
  content::WebContents* web_contents() { return web_contents_; }

  bool request_shipping() const { return options_->request_shipping; }
  bool request_payer_name() const { return options_->request_payer_name; }
  bool request_payer_phone() const { return options_->request_payer_phone; }
  bool request_payer_email() const { return options_->request_payer_email; }

  bool is_ready_to_pay() { return is_ready_to_pay_; }

 private:
  // Fetches the Autofill Profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this Request, in profile_cache_.
  void PopulateProfileCache();

  // Sets the default values for the selected Shipping and Contact profiles, as
  // well as the selected Credit Card.
  void SetDefaultProfileSelections();

  // Validates the |method_data| and fills |supported_card_networks_|.
  void PopulateValidatedMethodData(
      const std::vector<payments::mojom::PaymentMethodDataPtr>& method_data);

  // Updates |is_ready_to_pay_| with the current state, by validating that all
  // the required information is available and notify observers.
  void UpdateIsReadyToPayAndNotifyObservers();

  // Notifies all observers that selected information has changed.
  void NotifyOnSelectedInformationChanged();

  // Returns whether the selected data satisfies the PaymentDetails requirements
  // (payment methods).
  bool ArePaymentDetailsSatisfied();
  // Returns whether the selected data satisfies the PaymentOptions requirements
  // (contact info, shipping address).
  bool ArePaymentOptionsSatisfied();

  content::WebContents* web_contents_;
  std::unique_ptr<PaymentRequestDelegate> delegate_;
  // |manager_| owns this PaymentRequest.
  PaymentRequestWebContentsManager* manager_;
  mojo::Binding<payments::mojom::PaymentRequest> binding_;
  payments::mojom::PaymentRequestClientPtr client_;
  payments::mojom::PaymentDetailsPtr details_;
  payments::mojom::PaymentOptionsPtr options_;
  std::unique_ptr<CurrencyFormatter> currency_formatter_;
  // A set of supported basic card networks.
  std::vector<std::string> supported_card_networks_;
  bool is_ready_to_pay_;

  base::ObserverList<Observer> observers_;

  // Profiles may change due to (e.g.) sync events, so profiles are cached after
  // loading and owned here. They are populated once only, and ordered by
  // frecency.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;
  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  std::vector<autofill::AutofillProfile*> contact_profiles_;
  autofill::AutofillProfile* selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile_;
  std::vector<std::unique_ptr<autofill::CreditCard>> card_cache_;
  std::vector<autofill::CreditCard*> credit_cards_;
  autofill::CreditCard* selected_credit_card_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
