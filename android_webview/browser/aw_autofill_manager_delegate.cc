// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_autofill_manager_delegate.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_pref_store.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_builder.h"
#include "components/autofill/browser/autocheckout/whitelist_manager.h"
#include "components/autofill/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"

namespace {

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

}

namespace android_webview {

AwAutofillManagerDelegate::AwAutofillManagerDelegate(bool enabled) {
  PrefRegistrySimple* pref_registry = new PrefRegistrySimple();
  pref_registry->RegisterBooleanPref(
      autofill::prefs::kAutofillEnabled, enabled);
  pref_registry->RegisterDoublePref(
      autofill::prefs::kAutofillPositiveUploadRate, 0.0);
  pref_registry->RegisterDoublePref(
      autofill::prefs::kAutofillNegativeUploadRate, 0.0);

  PrefServiceBuilder pref_service_builder;
  pref_service_builder.WithUserPrefs(new AwPrefStore());
  pref_service_builder.WithReadErrorCallback(base::Bind(&HandleReadError));

  AwBrowserContext* context = AwContentBrowserClient::GetAwBrowserContext();
  components::UserPrefs::Set(context,
                             pref_service_builder.Create(pref_registry));
}

AwAutofillManagerDelegate::~AwAutofillManagerDelegate() { }

void AwAutofillManagerDelegate::SetSaveFormData(bool enabled) {
  PrefService* service = GetPrefs();
  DCHECK(service);
  service->SetBoolean(autofill::prefs::kAutofillEnabled, enabled);
}

bool AwAutofillManagerDelegate::GetSaveFormData() {
  PrefService* service = GetPrefs();
  DCHECK(service);
  return service->GetBoolean(autofill::prefs::kAutofillEnabled);
}

PrefService* AwAutofillManagerDelegate::GetPrefs() {
  return components::UserPrefs::Get(
      AwContentBrowserClient::GetAwBrowserContext());
}

autofill::PersonalDataManager*
AwAutofillManagerDelegate::GetPersonalDataManager() {
  return NULL;
}

autofill::autocheckout::WhitelistManager*
AwAutofillManagerDelegate::GetAutocheckoutWhitelistManager() const {
  return NULL;
}

void AwAutofillManagerDelegate::HideRequestAutocompleteDialog() {
}

void AwAutofillManagerDelegate::OnAutocheckoutError() {
}

void AwAutofillManagerDelegate::ShowAutofillSettings() {
}

void AwAutofillManagerDelegate::ConfirmSaveCreditCard(
    const autofill::AutofillMetrics& metric_logger,
    const autofill::CreditCard& credit_card,
    const base::Closure& save_card_callback) {
}

void AwAutofillManagerDelegate::ShowAutocheckoutBubble(
    const gfx::RectF& bounding_box,
    bool is_google_user,
    const base::Callback<void(bool)>& callback) {
}

void AwAutofillManagerDelegate::HideAutocheckoutBubble() {
}

void AwAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const autofill::FormData& form,
    const GURL& source_url,
    autofill::DialogType dialog_type,
    const base::Callback<void(const autofill::FormStructure*,
                              const std::string&)>& callback) {
}

void AwAutofillManagerDelegate::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
}

void AwAutofillManagerDelegate::HideAutofillPopup() {
}

void AwAutofillManagerDelegate::UpdateProgressBar(double value) {
}

} // namespace android_webview
