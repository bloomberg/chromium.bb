// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include <vector>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_combobox_model.h"
#include "components/autofill/core/browser/region_data_loader_impl.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/core/payment_prefs.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace payments {

namespace {

std::unique_ptr<::i18n::addressinput::Source> GetAddressInputSource(
    net::URLRequestContextGetter* url_context_getter) {
  return std::unique_ptr<::i18n::addressinput::Source>(
      new autofill::ChromeMetadataSource(I18N_ADDRESS_VALIDATION_DATA_URL,
                                         url_context_getter));
}

std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage() {
  return autofill::ValidationRulesStorageFactory::CreateStorage();
}

}  // namespace

ChromePaymentRequestDelegate::ChromePaymentRequestDelegate(
    content::WebContents* web_contents)
    : dialog_(nullptr),
      web_contents_(web_contents),
      address_normalizer_(
          GetAddressInputSource(
              GetPersonalDataManager()->GetURLRequestContextGetter()),
          GetAddressInputStorage()) {}

ChromePaymentRequestDelegate::~ChromePaymentRequestDelegate() {}

void ChromePaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  DCHECK_EQ(nullptr, dialog_);
  dialog_ = chrome::CreatePaymentRequestDialog(request);
  dialog_->ShowDialog();
}

void ChromePaymentRequestDelegate::CloseDialog() {
  if (dialog_) {
    dialog_->CloseDialog();
    dialog_ = nullptr;
  }
}

void ChromePaymentRequestDelegate::ShowErrorMessage() {
  if (dialog_)
    dialog_->ShowErrorMessage();
}

autofill::PersonalDataManager*
ChromePaymentRequestDelegate::GetPersonalDataManager() {
  // Autofill uses the original profile's PersonalDataManager to make data
  // available in incognito, so PaymentRequest should do the same.
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetOriginalProfile());
}

const std::string& ChromePaymentRequestDelegate::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

bool ChromePaymentRequestDelegate::IsIncognito() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile && profile->GetProfileType() == Profile::INCOGNITO_PROFILE;
}

bool ChromePaymentRequestDelegate::IsSslCertificateValid() {
  return SslValidityChecker::IsSslCertificateValid(web_contents_);
}

const GURL& ChromePaymentRequestDelegate::GetLastCommittedURL() const {
  return web_contents_->GetLastCommittedURL();
}

void ChromePaymentRequestDelegate::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  dialog_->ShowCvcUnmaskPrompt(credit_card, result_delegate, web_contents_);
}

autofill::RegionDataLoader*
ChromePaymentRequestDelegate::GetRegionDataLoader() {
  return new autofill::RegionDataLoaderImpl(
      GetAddressInputSource(
          GetPersonalDataManager()->GetURLRequestContextGetter())
          .release(),
      GetAddressInputStorage().release(), GetApplicationLocale());
}

AddressNormalizer* ChromePaymentRequestDelegate::GetAddressNormalizer() {
  return &address_normalizer_;
}

ukm::UkmRecorder* ChromePaymentRequestDelegate::GetUkmRecorder() {
  return g_browser_process->ukm_recorder();
}

std::string ChromePaymentRequestDelegate::GetAuthenticatedEmail() const {
  // Check if the profile is authenticated.  Guest profiles or incognito
  // windows may not have a sign in manager, and are considered not
  // authenticated.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  if (signin_manager && signin_manager->IsAuthenticated())
    return signin_manager->GetAuthenticatedAccountInfo().email;

  return std::string();
}

PrefService* ChromePaymentRequestDelegate::GetPrefService() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}

}  // namespace payments
