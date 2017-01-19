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
      binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(
      base::Bind(&PaymentRequest::OnError, base::Unretained(this)));
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
    OnError();
    client_.reset();
    return;
  }
  client_ = std::move(client);
  details_ = std::move(details);
}

void PaymentRequest::Show() {
  if (!client_.is_bound() || !binding_.is_bound()) {
    OnError();
    return;
  }
  delegate_->ShowPaymentRequestDialog(this);
}

void PaymentRequest::Cancel() {
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);
}

void PaymentRequest::OnError() {
  binding_.Close();
  manager_->DestroyRequest(this);
}

CurrencyFormatter* PaymentRequest::GetOrCreateCurrencyFormatter(
    const std::string& currency_code,
    const base::Optional<std::string> currency_system,
    const std::string& locale_name) {
  if (!currency_formatter_) {
    currency_formatter_.reset(
        new CurrencyFormatter(currency_code, currency_system, locale_name));
  }

  return currency_formatter_.get();
}

autofill::AutofillProfile* PaymentRequest::GetCurrentlySelectedProfile() {
  // TODO(tmartino): Implement more sophisticated algorithm for populating
  // this when it starts empty.
  if (!profile_) {
    autofill::PersonalDataManager* data_manager =
        delegate_->GetPersonalDataManager();
    auto profiles = data_manager->GetProfiles();
    if (!profiles.empty())
      profile_ = base::MakeUnique<autofill::AutofillProfile>(*profiles[0]);
  }
  return profile_ ? profile_.get() : nullptr;
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

}  // namespace payments
