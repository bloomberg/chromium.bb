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
#include "components/autofill/content/browser/autocheckout/whitelist_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(android_webview::AwAutofillManagerDelegate);

namespace android_webview {

AwAutofillManagerDelegate::AwAutofillManagerDelegate(WebContents* contents)
    : web_contents_(contents),
      save_form_data_(false) {
}

AwAutofillManagerDelegate::~AwAutofillManagerDelegate() { }

void AwAutofillManagerDelegate::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool AwAutofillManagerDelegate::GetSaveFormData() {
  return save_form_data_;
}

PrefService* AwAutofillManagerDelegate::GetPrefs() {
  return user_prefs::UserPrefs::Get(
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

void AwAutofillManagerDelegate::OnAutocheckoutSuccess() {
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
    base::i18n::TextDirection text_direction,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
}

void AwAutofillManagerDelegate::HideAutofillPopup() {
}

void AwAutofillManagerDelegate::AddAutocheckoutStep(
    autofill::AutocheckoutStepType step_type) {
}

void AwAutofillManagerDelegate::UpdateAutocheckoutStep(
    autofill::AutocheckoutStepType step_type,
    autofill::AutocheckoutStepStatus step_status) {
}

bool AwAutofillManagerDelegate::IsAutocompleteEnabled() {
  return GetSaveFormData();
}

} // namespace android_webview
