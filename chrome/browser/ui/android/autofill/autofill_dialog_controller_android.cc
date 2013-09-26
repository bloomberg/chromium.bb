// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_dialog_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "jni/AutofillDialogControllerAndroid_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// Keys in kAutofillDialogDefaults pref dictionary (do not change these values).
const char kLastUsedAccountName[] = "last_used_account_name";
const char kLastUsedChoiceIsAutofill[] = "last_used_choice_is_autofill";
const char kLastUsedBillingAddressGuid[] = "last_used_billing";
const char kLastUsedShippingAddressGuid[] = "last_used_shipping";
const char kLastUsedCreditCardGuid[] = "last_used_card";

scoped_ptr<DataModelWrapper> CreateWrapper(
    DialogSection section, wallet::FullWallet* full_wallet) {
  if (section == SECTION_CC_BILLING) {
    if (!full_wallet->billing_address())
      return scoped_ptr<DataModelWrapper>();

    return scoped_ptr<DataModelWrapper>(
        new FullWalletBillingWrapper(full_wallet));
  }
  if (section == SECTION_SHIPPING) {
    if (!full_wallet->shipping_address())
      return scoped_ptr<DataModelWrapper>();

    return scoped_ptr<DataModelWrapper>(
        new FullWalletShippingWrapper(full_wallet));
  }
  NOTREACHED();
  return scoped_ptr<DataModelWrapper>();
}

void FillOutputForSectionWithComparator(
    DialogSection section, const DetailInputs& inputs,
    const InputFieldComparator& compare,
    FormStructure& form_structure, wallet::FullWallet* full_wallet,
    const base::string16& email_address) {
  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section, full_wallet);
  if (wrapper)
    wrapper->FillFormStructure(inputs, compare, &form_structure);
}

void FillOutputForSection(
    DialogSection section,
    FormStructure& form_structure,
    wallet::FullWallet* full_wallet,
    const base::string16& email_address) {
  DetailInputs inputs;
  common::BuildInputsForSection(section, &inputs);

  FillOutputForSectionWithComparator(
      section, inputs,
      base::Bind(common::DetailInputMatchesField, section),
      form_structure, full_wallet, email_address);

  if (section == SECTION_CC_BILLING) {
    // Email is hidden while using Wallet, special case it.
    for (size_t i = 0; i < form_structure.field_count(); ++i) {
      AutofillField* field = form_structure.field(i);
      if (field->Type().GetStorableType() == EMAIL_ADDRESS)
        field->value = email_address;
    }
  }
}

// Returns true if |input_type| in |section| is needed for |form_structure|.
bool IsSectionInputUsedInFormStructure(DialogSection section,
                                       ServerFieldType input_type,
                                       const FormStructure& form_structure) {
  const DetailInput input = { 0, input_type };
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    const AutofillField* field = form_structure.field(i);
    if (field && common::DetailInputMatchesField(section, input, *field))
      return true;
  }
  return false;
}

// Returns true if one of |inputs| in |section| is needed for |form_structure|.
bool IsSectionInputsUsedInFormStructure(DialogSection section,
                                        const ServerFieldType* input_types,
                                        const size_t input_types_size,
                                        const FormStructure& form_structure) {
  for (size_t i = 0; i < input_types_size; ++i) {
    if (IsSectionInputUsedInFormStructure(
        section, input_types[i], form_structure)) {
      return true;
    }
  }
  return false;
}

}  // namespace


// static
base::WeakPtr<AutofillDialogController> AutofillDialogControllerAndroid::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback) {
  // AutofillDialogControllerAndroid owns itself.
  AutofillDialogControllerAndroid* autofill_dialog_controller =
      new AutofillDialogControllerAndroid(contents,
                                          form_structure,
                                          source_url,
                                          callback);
  return autofill_dialog_controller->weak_ptr_factory_.GetWeakPtr();
}

// static
void AutofillDialogControllerAndroid::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      ::prefs::kAutofillDialogDefaults,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
base::WeakPtr<AutofillDialogController>
AutofillDialogController::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback) {
  return AutofillDialogControllerAndroid::Create(contents,
                                                 form_structure,
                                                 source_url,
                                                 callback);
}

// static
void AutofillDialogController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AutofillDialogControllerAndroid::RegisterProfilePrefs(registry);
}

AutofillDialogControllerAndroid::~AutofillDialogControllerAndroid() {
  if (java_object_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogControllerAndroid_onDestroy(env, java_object_.obj());
}

void AutofillDialogControllerAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  dialog_shown_timestamp_ = base::Time::Now();

  content::NavigationEntry* entry = contents_->GetController().GetActiveEntry();
  const GURL& active_url = entry ? entry->GetURL() : contents_->GetURL();
  invoked_from_same_origin_ = active_url.GetOrigin() == source_url_.GetOrigin();

  // Determine what field types should be included in the dialog.
  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(
      &has_types, &has_sections);

  // Fail if the author didn't specify autocomplete types, or
  // if the dialog shouldn't be shown in a given circumstances.
  if (!has_types ||
      !Java_AutofillDialogControllerAndroid_isDialogAllowed(
          env,
          RequestingCreditCardInfo(),
          TransmissionWillBeSecure(),
          invoked_from_same_origin_)) {
    callback_.Run(NULL);
    delete this;
    return;
  }

  // Log any relevant UI metrics and security exceptions.
  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_SHOWN);

  GetMetricLogger().LogDialogSecurityMetric(
      AutofillMetrics::SECURITY_METRIC_DIALOG_SHOWN);

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    GetMetricLogger().LogDialogSecurityMetric(
        AutofillMetrics::SECURITY_METRIC_CREDIT_CARD_OVER_HTTP);
  }

  if (!invoked_from_same_origin_) {
    GetMetricLogger().LogDialogSecurityMetric(
        AutofillMetrics::SECURITY_METRIC_CROSS_ORIGIN_FRAME);
  }

  const ServerFieldType full_billing_is_necessary_if[] = {
      ADDRESS_BILLING_LINE1,
      ADDRESS_BILLING_LINE2,
      ADDRESS_BILLING_CITY,
      ADDRESS_BILLING_STATE,
      PHONE_BILLING_WHOLE_NUMBER
  };
  const bool request_full_billing_address =
      IsSectionInputsUsedInFormStructure(
          SECTION_BILLING,
          full_billing_is_necessary_if,
          arraysize(full_billing_is_necessary_if),
          form_structure_);
  const bool request_phone_numbers =
      IsSectionInputUsedInFormStructure(
          SECTION_BILLING,
          PHONE_BILLING_WHOLE_NUMBER,
          form_structure_) ||
      IsSectionInputUsedInFormStructure(
          SECTION_SHIPPING,
          PHONE_HOME_WHOLE_NUMBER,
          form_structure_);

  bool request_shipping_address = false;
  {
    DetailInputs inputs;
    common::BuildInputsForSection(SECTION_SHIPPING, &inputs);
    EmptyDataModelWrapper empty_wrapper;
    request_shipping_address = empty_wrapper.FillFormStructure(
        inputs,
        base::Bind(common::DetailInputMatchesField, SECTION_SHIPPING),
        &form_structure_);
  }

  const bool incognito_mode = profile_->IsOffTheRecord();

  bool last_used_choice_is_autofill = false;
  base::string16 last_used_account_name;
  std::string last_used_billing;
  std::string last_used_shipping;
  std::string last_used_credit_card;
  {
    const base::DictionaryValue* defaults =
        profile_->GetPrefs()->GetDictionary(::prefs::kAutofillDialogDefaults);
    if (defaults) {
      defaults->GetString(kLastUsedAccountName, &last_used_account_name);
      defaults->GetBoolean(kLastUsedChoiceIsAutofill,
                           &last_used_choice_is_autofill);
      defaults->GetString(kLastUsedBillingAddressGuid, &last_used_billing);
      defaults->GetString(kLastUsedShippingAddressGuid, &last_used_shipping);
      defaults->GetString(kLastUsedCreditCardGuid, &last_used_credit_card);
    } else {
      DLOG(ERROR) << "Failed to read AutofillDialog preferences";
    }
  }

  if (contents_->GetBrowserContext()->IsOffTheRecord())
    last_used_choice_is_autofill = true;

  ScopedJavaLocalRef<jstring> jlast_used_account_name =
      base::android::ConvertUTF16ToJavaString(
          env, last_used_account_name);
  ScopedJavaLocalRef<jstring> jlast_used_billing =
      base::android::ConvertUTF8ToJavaString(
          env, last_used_billing);
  ScopedJavaLocalRef<jstring> jlast_used_shipping =
      base::android::ConvertUTF8ToJavaString(
          env, last_used_shipping);
  ScopedJavaLocalRef<jstring> jlast_used_card =
      base::android::ConvertUTF8ToJavaString(
          env, last_used_credit_card);
  ScopedJavaLocalRef<jstring> jmerchant_domain =
      base::android::ConvertUTF8ToJavaString(
          env, source_url_.GetOrigin().spec());
  java_object_.Reset(Java_AutofillDialogControllerAndroid_create(
      env,
      reinterpret_cast<jint>(this),
      WindowAndroidHelper::FromWebContents(contents_)->
          GetWindowAndroid()->GetJavaObject().obj(),
      request_full_billing_address, request_shipping_address,
      request_phone_numbers, incognito_mode,
      last_used_choice_is_autofill, jlast_used_account_name.obj(),
      jlast_used_billing.obj(), jlast_used_shipping.obj(),
      jlast_used_card.obj(),
      jmerchant_domain.obj()));
}

void AutofillDialogControllerAndroid::Hide() {
  // TODO(aruslan): http://crbug.com/177373 Autocheckout.
  NOTIMPLEMENTED();
}

void AutofillDialogControllerAndroid::TabActivated() {}

// static
bool AutofillDialogControllerAndroid::
    RegisterAutofillDialogControllerAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AutofillDialogControllerAndroid::DialogCancel(JNIEnv* env,
                                                   jobject obj) {
  LogOnCancelMetrics();
  callback_.Run(NULL);
}

void AutofillDialogControllerAndroid::DialogContinue(
    JNIEnv* env,
    jobject obj,
    jobject wallet,
    jboolean jlast_used_choice_is_autofill,
    jstring jlast_used_account_name,
    jstring jlast_used_billing,
    jstring jlast_used_shipping,
    jstring jlast_used_card) {
  const string16 email = AutofillDialogResult::GetWalletEmail(env, wallet);
  const std::string google_transaction_id =
      AutofillDialogResult::GetWalletGoogleTransactionId(env, wallet);

  const string16 last_used_account_name =
      base::android::ConvertJavaStringToUTF16(env, jlast_used_account_name);
  const std::string last_used_billing =
      base::android::ConvertJavaStringToUTF8(env, jlast_used_billing);
  const std::string last_used_shipping =
      base::android::ConvertJavaStringToUTF8(env, jlast_used_shipping);
  const std::string last_used_card =
      base::android::ConvertJavaStringToUTF8(env, jlast_used_card);

  scoped_ptr<wallet::FullWallet> full_wallet =
      AutofillDialogResult::ConvertFromJava(env, wallet);
  FillOutputForSection(
      SECTION_CC_BILLING, form_structure_, full_wallet.get(), email);
  FillOutputForSection(
      SECTION_SHIPPING, form_structure_, full_wallet.get(), email);

  {
    DictionaryPrefUpdate updater(profile_->GetPrefs(),
                                 ::prefs::kAutofillDialogDefaults);
    base::DictionaryValue* defaults = updater.Get();
    if (defaults) {
      const bool last_used_choice_is_autofill = !!jlast_used_choice_is_autofill;
      defaults->SetString(kLastUsedAccountName, last_used_account_name);
      defaults->SetBoolean(kLastUsedChoiceIsAutofill,
                           last_used_choice_is_autofill);
      if (!last_used_billing.empty())
        defaults->SetString(kLastUsedBillingAddressGuid, last_used_billing);
      if (!last_used_shipping.empty())
        defaults->SetString(kLastUsedShippingAddressGuid, last_used_shipping);
      if (!last_used_card.empty())
        defaults->SetString(kLastUsedCreditCardGuid, last_used_card);
    } else {
      LOG(ERROR) << "Failed to save AutofillDialog preferences";
    }
  }

  LogOnFinishSubmitMetrics();

  // Callback should be called as late as possible.
  callback_.Run(&form_structure_);

  // This might delete us.
  Hide();
}

AutofillDialogControllerAndroid::AutofillDialogControllerAndroid(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback)
    : profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      contents_(contents),
      initial_user_state_(AutofillMetrics::DIALOG_USER_STATE_UNKNOWN),
      form_structure_(form_structure),
      invoked_from_same_origin_(true),
      source_url_(source_url),
      callback_(callback),
      cares_about_shipping_(true),
      weak_ptr_factory_(this),
      was_ui_latency_logged_(false) {
  DCHECK(!callback_.is_null());
}

bool AutofillDialogControllerAndroid::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillType type = form_structure_.field(i)->Type();
    if (common::IsCreditCardType(type.GetStorableType()))
      return true;
  }

  return false;
}

bool AutofillDialogControllerAndroid::TransmissionWillBeSecure() const {
  return source_url_.SchemeIs(content::kHttpsScheme);
}

void AutofillDialogControllerAndroid::LogOnFinishSubmitMetrics() {
  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      AutofillMetrics::DIALOG_ACCEPTED);

  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_ACCEPTED);
}

void AutofillDialogControllerAndroid::LogOnCancelMetrics() {
  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      AutofillMetrics::DIALOG_CANCELED);

  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_CANCELED);
}

}  // namespace autofill
