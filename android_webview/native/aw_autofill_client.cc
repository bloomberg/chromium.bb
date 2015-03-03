// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_autofill_client.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_pref_store.h"
#include "android_webview/native/aw_contents.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwAutofillClient_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(android_webview::AwAutofillClient);

namespace android_webview {

// Ownership: The native object is created (if autofill enabled) and owned by
// AwContents. The native object creates the java peer which handles most
// autofill functionality at the java side. The java peer is owned by Java
// AwContents. The native object only maintains a weak ref to it.
AwAutofillClient::AwAutofillClient(WebContents* contents)
    : web_contents_(contents), save_form_data_(false) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate;
  delegate.Reset(
      Java_AwAutofillClient_create(env, reinterpret_cast<intptr_t>(this)));

  AwContents* aw_contents = AwContents::FromWebContents(web_contents_);
  aw_contents->SetAwAutofillClient(delegate.obj());
  java_ref_ = JavaObjectWeakGlobalRef(env, delegate.obj());
}

AwAutofillClient::~AwAutofillClient() {
  HideAutofillPopup();
}

void AwAutofillClient::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool AwAutofillClient::GetSaveFormData() {
  return save_form_data_;
}

PrefService* AwAutofillClient::GetPrefs() {
  return user_prefs::UserPrefs::Get(
      AwContentBrowserClient::GetAwBrowserContext());
}

IdentityProvider* AwAutofillClient::GetIdentityProvider() {
  return nullptr;
}

autofill::PersonalDataManager* AwAutofillClient::GetPersonalDataManager() {
  return nullptr;
}

scoped_refptr<autofill::AutofillWebDataService>
AwAutofillClient::GetDatabase() {
  android_webview::AwFormDatabaseService* service =
      static_cast<android_webview::AwBrowserContext*>(
          web_contents_->GetBrowserContext())->GetFormDatabaseService();
  return service->get_autofill_webdata_service();
}

void AwAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<autofill::Suggestion>& suggestions,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  suggestions_ = suggestions;
  delegate_ = delegate;

  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents_->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  ShowAutofillPopupImpl(element_bounds_in_screen_space,
                        text_direction == base::i18n::RIGHT_TO_LEFT,
                        suggestions);
}

void AwAutofillClient::ShowAutofillPopupImpl(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // We need an array of AutofillSuggestion.
  size_t count = suggestions.size();

  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AwAutofillClient_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> name =
        ConvertUTF16ToJavaString(env, suggestions[i].value);
    ScopedJavaLocalRef<jstring> label =
        ConvertUTF16ToJavaString(env, suggestions[i].label);
    Java_AwAutofillClient_addToAutofillSuggestionArray(
        env, data_array.obj(), i, name.obj(), label.obj(),
        suggestions[i].frontend_id);
  }

  Java_AwAutofillClient_showAutofillPopup(env,
                                          obj.obj(),
                                          element_bounds.x(),
                                          element_bounds.y(),
                                          element_bounds.width(),
                                          element_bounds.height(),
                                          is_rtl,
                                          data_array.obj());
}

void AwAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  // Leaving as an empty method since updating autofill popup window
  // dynamically does not seem to be a useful feature for android webview.
  // See crrev.com/18102002 if need to implement.
}

void AwAutofillClient::HideAutofillPopup() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  delegate_.reset();
  Java_AwAutofillClient_hideAutofillPopup(env, obj.obj());
}

bool AwAutofillClient::IsAutocompleteEnabled() {
  return GetSaveFormData();
}

void AwAutofillClient::DetectAccountCreationForms(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {

}

void AwAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
}

void AwAutofillClient::OnFirstUserGestureObserved() {
  NOTIMPLEMENTED();
}

void AwAutofillClient::LinkClicked(const GURL& url,
                                   WindowOpenDisposition disposition) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::SuggestionSelected(JNIEnv* env,
                                          jobject object,
                                          jint position) {
  if (delegate_) {
    delegate_->DidAcceptSuggestion(suggestions_[position].value,
                                   suggestions_[position].frontend_id);
  }
}

void AwAutofillClient::HideRequestAutocompleteDialog() {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowAutofillSettings() {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::OnUnmaskVerificationResult(GetRealPanResult result) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveCreditCard(
    const base::Closure& save_card_callback) {
  NOTIMPLEMENTED();
}

bool AwAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void AwAutofillClient::ScanCreditCard(const CreditCardScanCallback& callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowRequestAutocompleteDialog(
    const autofill::FormData& form,
    content::RenderFrameHost* rfh,
    const ResultCallback& callback) {
  NOTIMPLEMENTED();
}

bool RegisterAwAutofillClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
