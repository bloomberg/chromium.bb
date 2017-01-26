// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "components/payments/currency_formatter.h"
#include "components/payments/payment_request.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequestDelegate;
class PaymentRequestWebContentsManager;

class PaymentRequest : payments::mojom::PaymentRequest {
 public:
  PaymentRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PaymentRequestDelegate> delegate,
      PaymentRequestWebContentsManager* manager,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);
  ~PaymentRequest() override;

  // payments::mojom::PaymentRequest "stub"
  void Init(payments::mojom::PaymentRequestClientPtr client,
            std::vector<payments::mojom::PaymentMethodDataPtr> methodData,
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
  const std::vector<autofill::AutofillProfile*>& shipping_profiles();
  const std::vector<autofill::AutofillProfile*>& contact_profiles();

  // Gets/sets the Autofill Profile representing the shipping address or contact
  // information currently selected for this PaymentRequest flow. Can return
  // null.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }
  void set_selected_shipping_profile(autofill::AutofillProfile* profile) {
    selected_shipping_profile_ = profile;
  }
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }
  void set_selected_contact_profile(autofill::AutofillProfile* profile) {
    selected_contact_profile_ = profile;
  }

  // Returns the currently selected credit card for this PaymentRequest flow.
  // It's not guaranteed to be complete. Returns nullptr if there is no selected
  // card.
  autofill::CreditCard* GetCurrentlySelectedCreditCard();

  payments::mojom::PaymentDetails* details() { return details_.get(); }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // Fetches the Autofill Profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this Request, in profile_cache_.
  void PopulateProfileCache();

  // Sets the default values for the selected Shipping and Contact profiles.
  void SetDefaultProfileSelections();

  content::WebContents* web_contents_;
  std::unique_ptr<PaymentRequestDelegate> delegate_;
  // |manager_| owns this PaymentRequest.
  PaymentRequestWebContentsManager* manager_;
  mojo::Binding<payments::mojom::PaymentRequest> binding_;
  payments::mojom::PaymentRequestClientPtr client_;
  payments::mojom::PaymentDetailsPtr details_;
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  // Profiles may change due to (e.g.) sync events, so profiles are cached after
  // loading and owned here. They are populated once only, and ordered by
  // frecency.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;
  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  std::vector<autofill::AutofillProfile*> contact_profiles_;
  autofill::AutofillProfile* selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_
