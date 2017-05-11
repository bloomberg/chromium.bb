// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include <vector>

#include "base/timer/timer.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_combobox_model.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/core/payment_prefs.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data_builder.h"

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

// A wrapper for the PreloadSupplier to simplify testing. Until crbug.com/712832
// is fixed, to avoid leaks instances of this class are left hanging until the
// callback is Run. If the callback is never run, this class will leak. But even
// if we would prevent this class from leaking, the preload supplier will leak
// data anyway if the load never completes. This class gives us more opportunity
// to prevent that preload supplier leak if the data load happens after the UI
// timeout waiting for this data expires.
class RegionDataLoaderImpl : public autofill::RegionDataLoader {
 public:
  RegionDataLoaderImpl(net::URLRequestContextGetter* url_context_getter,
                       const std::string& app_locale)
      // region_data_supplier_ takes ownership of source and storage.
      : region_data_supplier_(
            GetAddressInputSource(url_context_getter).release(),
            GetAddressInputStorage().release()),
        app_locale_(app_locale) {
    region_data_supplier_callback_.reset(::i18n::addressinput::BuildCallback(
        this, &RegionDataLoaderImpl::OnRegionDataLoaded));
  }

  // autofill::RegionDataLoader.
  void LoadRegionData(const std::string& country_code,
                      autofill::RegionDataLoader::RegionDataLoaded callback,
                      int64_t timeout_ms) override {
    callback_ = callback;
    region_data_supplier_.LoadRules(country_code,
                                    *region_data_supplier_callback_.get());

    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_ms),
                 base::Bind(&RegionDataLoaderImpl::OnRegionDataLoaded,
                            base::Unretained(this), false, country_code, 0));
  }
  void ClearCallback() override { callback_.Reset(); }

 private:
  void OnRegionDataLoaded(bool success,
                          const std::string& country_code,
                          int unused_rule_count) {
    timer_.Stop();
    if (!callback_.is_null()) {
      if (success) {
        std::string best_region_tree_language_tag;
        ::i18n::addressinput::RegionDataBuilder builder(&region_data_supplier_);
        callback_.Run(builder
                          .Build(country_code, app_locale_,
                                 &best_region_tree_language_tag)
                          .sub_regions());
      } else {
        callback_.Run(std::vector<const ::i18n::addressinput::RegionData*>());
      }
    }
    // The deletion must be asynchronous since the caller is not quite done with
    // the preload supplier.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RegionDataLoaderImpl::DeleteThis, base::Unretained(this)));
  }

  void DeleteThis() { delete this; }

  // The callback to give to |region_data_supplier_| for async operations.
  ::i18n::addressinput::scoped_ptr<
      ::i18n::addressinput::PreloadSupplier::Callback>
      region_data_supplier_callback_;

  // A supplier of region data.
  ::i18n::addressinput::PreloadSupplier region_data_supplier_;

  std::string app_locale_;
  autofill::RegionDataLoader::RegionDataLoaded callback_;
  base::OneShotTimer timer_;
};

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
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
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
  return new RegionDataLoaderImpl(
      GetPersonalDataManager()->GetURLRequestContextGetter(),
      GetApplicationLocale());
}

AddressNormalizer* ChromePaymentRequestDelegate::GetAddressNormalizer() {
  return &address_normalizer_;
}

ukm::UkmService* ChromePaymentRequestDelegate::GetUkmService() {
  return g_browser_process->ukm_service();
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
