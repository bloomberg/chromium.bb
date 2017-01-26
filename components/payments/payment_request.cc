// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/payment_request.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/payment_details_validation.h"
#include "components/payments/payment_request_delegate.h"
#include "components/payments/payment_request_web_contents_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentRequest::PaymentRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      manager_(manager),
      binding_(this, std::move(request)),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr) {
  // OnConnectionTerminated will be called when the Mojo pipe is closed. This
  // will happen as a result of many renderer-side events (both successful and
  // erroneous in nature).
  // TODO(crbug.com/683636): Investigate using
  // set_connection_error_with_reason_handler with Binding::CloseWithReason.
  binding_.set_connection_error_handler(base::Bind(
      &PaymentRequest::OnConnectionTerminated, base::Unretained(this)));

}

PaymentRequest::~PaymentRequest() {}

void PaymentRequest::Init(
    payments::mojom::PaymentRequestClientPtr client,
    std::vector<payments::mojom::PaymentMethodDataPtr> methodData,
    payments::mojom::PaymentDetailsPtr details,
    payments::mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string error;
  if (!payments::validatePaymentDetails(details, &error)) {
    LOG(ERROR) << error;
    OnConnectionTerminated();
    return;
  }
  client_ = std::move(client);
  details_ = std::move(details);
  PopulateProfileCache();
  SetDefaultProfileSelections();
}

void PaymentRequest::Show() {
  if (!client_.is_bound() || !binding_.is_bound()) {
    LOG(ERROR) << "Attempted Show(), but binding(s) missing.";
    OnConnectionTerminated();
    return;
  }
  delegate_->ShowDialog(this);
}

void PaymentRequest::Abort() {
  // The API user has decided to abort. We return a successful abort message to
  // the renderer, which closes the Mojo message pipe, which triggers
  // PaymentRequest::OnConnectionTerminated, which destroys this object.
  if (client_.is_bound())
    client_->OnAbort(true /* aborted_successfully */);
}

void PaymentRequest::UserCancelled() {
  // If |client_| is not bound, then the object is already being destroyed as
  // a result of a renderer event.
  if (!client_.is_bound())
    return;

  // This sends an error to the renderer, which informs the API user.
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);

  // We close all bindings and ask to be destroyed.
  client_.reset();
  binding_.Close();
  manager_->DestroyRequest(this);
}

void PaymentRequest::OnConnectionTerminated() {
  // We are here because of a browser-side error, or likely as a result of the
  // connection_error_handler on |binding_|, which can mean that the renderer
  // has decided to close the pipe for various reasons (see all uses of
  // PaymentRequest::clearResolversAndCloseMojoConnection() in Blink). We close
  // the binding and the dialog, and ask to be deleted.
  client_.reset();
  binding_.Close();
  delegate_->CloseDialog();
  manager_->DestroyRequest(this);
}

CurrencyFormatter* PaymentRequest::GetOrCreateCurrencyFormatter(
    const std::string& currency_code,
    const std::string& currency_system,
    const std::string& locale_name) {
  if (!currency_formatter_) {
    currency_formatter_.reset(
        new CurrencyFormatter(currency_code, currency_system, locale_name));
  }
  return currency_formatter_.get();
}

const std::vector<autofill::AutofillProfile*>&
    PaymentRequest::shipping_profiles() {
  return shipping_profiles_;
}

const std::vector<autofill::AutofillProfile*>&
    PaymentRequest::contact_profiles() {
  return contact_profiles_;
}

autofill::CreditCard* PaymentRequest::GetCurrentlySelectedCreditCard() {
  // TODO(anthonyvd): Change this code to prioritize server cards and implement
  // a way to modify this function's return value.
  autofill::PersonalDataManager* data_manager =
      delegate_->GetPersonalDataManager();

  const std::vector<autofill::CreditCard*> cards =
      data_manager->GetCreditCardsToSuggest();

  auto first_complete_card = std::find_if(
      cards.begin(),
      cards.end(),
      [] (autofill::CreditCard* card) {
        return card->IsValid();
  });

  return first_complete_card == cards.end() ? nullptr : *first_complete_card;
}

void PaymentRequest::PopulateProfileCache() {
  autofill::PersonalDataManager* data_manager =
      delegate_->GetPersonalDataManager();
  std::vector<autofill::AutofillProfile*> profiles =
      data_manager->GetProfilesToSuggest();

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profiles[i]));

    // TODO(tmartino): Implement deduplication rules specific to shipping and
    // contact profiles.
    shipping_profiles_.push_back(profile_cache_[i].get());
    contact_profiles_.push_back(profile_cache_[i].get());
  }
}

void PaymentRequest::SetDefaultProfileSelections() {
  if (!shipping_profiles().empty())
    set_selected_shipping_profile(shipping_profiles()[0]);

  if (!contact_profiles().empty())
    set_selected_contact_profile(contact_profiles()[0]);
}

}  // namespace payments
