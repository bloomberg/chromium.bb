// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_details_validation.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/currency_formatter.h"
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
      is_ready_to_pay_(false),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_credit_card_(nullptr) {
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
    std::vector<payments::mojom::PaymentMethodDataPtr> method_data,
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
  options_ = std::move(options);
  PopulateValidatedMethodData(method_data);
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

void PaymentRequest::Pay() {
  DCHECK(is_ready_to_pay_);

  // TODO(mathp): Return the PaymentResponse to the |client_|.
  OnConnectionTerminated();
}

void PaymentRequest::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequest::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

void PaymentRequest::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile) {
  selected_shipping_profile_ = profile;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequest::SetSelectedContactProfile(
    autofill::AutofillProfile* profile) {
  selected_contact_profile_ = profile;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequest::SetSelectedCreditCard(autofill::CreditCard* card) {
  selected_credit_card_ = card;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequest::PopulateProfileCache() {
  std::vector<autofill::AutofillProfile*> profiles =
      personal_data_manager()->GetProfilesToSuggest();

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested. The same is true for credit cards.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        base::MakeUnique<autofill::AutofillProfile>(*profiles[i]));

    // TODO(tmartino): Implement deduplication rules specific to shipping and
    // contact profiles.
    shipping_profiles_.push_back(profile_cache_[i].get());
    contact_profiles_.push_back(profile_cache_[i].get());
  }

  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager()->GetCreditCardsToSuggest();
  for (autofill::CreditCard* card : cards) {
    card_cache_.push_back(base::MakeUnique<autofill::CreditCard>(*card));
    credit_cards_.push_back(card_cache_.back().get());
  }
}

void PaymentRequest::SetDefaultProfileSelections() {
  if (!shipping_profiles().empty())
    selected_shipping_profile_ = shipping_profiles()[0];

  if (!contact_profiles().empty())
    selected_contact_profile_ = contact_profiles()[0];

  // TODO(anthonyvd): Change this code to prioritize server cards and implement
  // a way to modify this function's return value.
  const std::vector<autofill::CreditCard*> cards = credit_cards();
  auto first_complete_card =
      std::find_if(cards.begin(), cards.end(),
                   [](autofill::CreditCard* card) { return card->IsValid(); });

  selected_credit_card_ =
      first_complete_card == cards.end() ? nullptr : *first_complete_card;

  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequest::PopulateValidatedMethodData(
    const std::vector<payments::mojom::PaymentMethodDataPtr>& method_data) {
  if (method_data.empty()) {
    LOG(ERROR) << "Invalid payment methods or data";
    OnConnectionTerminated();
    return;
  }

  // Identifier for the basic card payment method in the PaymentMethodData.
  const char* const kBasicCardMethodName = "basic-card";
  std::set<std::string> card_networks{"amex",     "diners",     "discover",
                                      "jcb",      "mastercard", "mir",
                                      "unionpay", "visa"};
  for (const payments::mojom::PaymentMethodDataPtr& method_data_entry :
       method_data) {
    std::vector<std::string> supported_methods =
        method_data_entry->supported_methods;
    if (supported_methods.empty()) {
      LOG(ERROR) << "Invalid payment methods or data";
      OnConnectionTerminated();
      return;
    }

    for (const std::string& method : supported_methods) {
      if (method.empty())
        continue;

      // If a card network is specified right in "supportedMethods", add it.
      auto card_it = card_networks.find(method);
      if (card_it != card_networks.end()) {
        supported_card_networks_.push_back(method);
        // |method| removed from |card_networks| so that it is not doubly added
        // to |supported_card_networks_| if "basic-card" is specified with no
        // supported networks.
        card_networks.erase(card_it);
      } else if (method == kBasicCardMethodName) {
        // For the "basic-card" method, check "supportedNetworks".
        if (method_data_entry->supported_networks.empty()) {
          // Empty |supported_networks| means all networks are supported.
          supported_card_networks_.insert(supported_card_networks_.end(),
                                          card_networks.begin(),
                                          card_networks.end());
          // Clear the set so that no further networks are added to
          // |supported_card_networks_|.
          card_networks.clear();
        } else {
          // The merchant has specified a few basic card supported networks. Use
          // the mapping to transform to known basic-card types.
          using ::payments::mojom::BasicCardNetwork;
          std::unordered_map<BasicCardNetwork, std::string> networks = {
              {BasicCardNetwork::AMEX, "amex"},
              {BasicCardNetwork::DINERS, "diners"},
              {BasicCardNetwork::DISCOVER, "discover"},
              {BasicCardNetwork::JCB, "jcb"},
              {BasicCardNetwork::MASTERCARD, "mastercard"},
              {BasicCardNetwork::MIR, "mir"},
              {BasicCardNetwork::UNIONPAY, "unionpay"},
              {BasicCardNetwork::VISA, "visa"}};
          for (const BasicCardNetwork& supported_network :
               method_data_entry->supported_networks) {
            // Make sure that the network was not already added to
            // |supported_card_networks_|.
            auto card_it = card_networks.find(networks[supported_network]);
            if (card_it != card_networks.end()) {
              supported_card_networks_.push_back(networks[supported_network]);
              card_networks.erase(card_it);
            }
          }
        }
      }
    }
  }
}

void PaymentRequest::UpdateIsReadyToPayAndNotifyObservers() {
  is_ready_to_pay_ =
      ArePaymentDetailsSatisfied() && ArePaymentOptionsSatisfied();
  NotifyOnSelectedInformationChanged();
}

void PaymentRequest::NotifyOnSelectedInformationChanged() {
  for (auto& observer : observers_)
    observer.OnSelectedInformationChanged();
}

bool PaymentRequest::ArePaymentDetailsSatisfied() {
  // TODO(mathp): A masked card may not satisfy IsValid().
  if (selected_credit_card_ == nullptr || !selected_credit_card_->IsValid())
    return false;

  const std::string basic_card_payment_type =
      autofill::data_util::GetPaymentRequestData(selected_credit_card_->type())
          .basic_card_payment_type;
  return !supported_card_networks_.empty() &&
         std::find(supported_card_networks_.begin(),
                   supported_card_networks_.end(),
                   basic_card_payment_type) != supported_card_networks_.end();
}

bool PaymentRequest::ArePaymentOptionsSatisfied() {
  // TODO(mathp): Have a measure of shipping address completeness.
  if (request_shipping() && selected_shipping_profile_ == nullptr)
    return false;

  // TODO(mathp): Make an encompassing class to validate contact info.
  const std::string& app_locale = delegate_->GetApplicationLocale();
  if (request_payer_name() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::NAME_FULL), app_locale)
           .empty())) {
    return false;
  }
  if (request_payer_email() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                     app_locale)
           .empty())) {
    return false;
  }
  if (request_payer_phone() &&
      (selected_contact_profile_ == nullptr ||
       selected_contact_profile_
           ->GetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                     app_locale)
           .empty())) {
    return false;
  }

  return true;
}

}  // namespace payments
