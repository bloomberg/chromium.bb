// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/personal_data_manager_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/format_macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "jni/PersonalDataManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace autofill {
namespace {

PrefService* GetPrefs() {
  return
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();
}

ScopedJavaLocalRef<jobject> CreateJavaProfileFromNative(
    JNIEnv* env,
    const AutofillProfile& profile) {
  return Java_AutofillProfile_create(
      env,
      ConvertUTF8ToJavaString(env, profile.guid()).obj(),
      ConvertUTF8ToJavaString(env, profile.origin()).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(NAME_FULL)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(COMPANY_NAME)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_STATE))
          .obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_CITY))
          .obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_ZIP)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_COUNTRY))
          .obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))
          .obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(EMAIL_ADDRESS)).obj(),
      ConvertUTF8ToJavaString(env, profile.language_code()).obj());
}

void PopulateNativeProfileFromJava(
    const jobject& jprofile,
    JNIEnv* env,
    AutofillProfile* profile) {
  profile->set_origin(
      ConvertJavaStringToUTF8(
          Java_AutofillProfile_getOrigin(env, jprofile)));
  profile->SetInfo(
      AutofillType(NAME_FULL),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getFullName(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(COMPANY_NAME),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getCompanyName(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getStreetAddress(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_STATE),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getRegion(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_CITY),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getLocality(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getDependentLocality(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_ZIP),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getPostalCode(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(ADDRESS_HOME_SORTING_CODE),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getSortingCode(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                   ConvertJavaStringToUTF16(
                       Java_AutofillProfile_getCountryCode(env, jprofile)),
                   g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(PHONE_HOME_WHOLE_NUMBER),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getPhoneNumber(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->SetInfo(
      AutofillType(EMAIL_ADDRESS),
      ConvertJavaStringToUTF16(
          Java_AutofillProfile_getEmailAddress(env, jprofile)),
      g_browser_process->GetApplicationLocale());
  profile->set_language_code(
      ConvertJavaStringToUTF8(
          Java_AutofillProfile_getLanguageCode(env, jprofile)));
}

ScopedJavaLocalRef<jobject> CreateJavaCreditCardFromNative(
    JNIEnv* env,
    const CreditCard& card) {
  return Java_CreditCard_create(
      env,
      ConvertUTF8ToJavaString(env, card.guid()).obj(),
      ConvertUTF8ToJavaString(env, card.origin()).obj(),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NAME)).obj(),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NUMBER)).obj(),
      ConvertUTF16ToJavaString(env, card.ObfuscatedNumber()).obj(),
      ConvertUTF16ToJavaString(
          env,
          card.GetRawInfo(CREDIT_CARD_EXP_MONTH)).obj(),
      ConvertUTF16ToJavaString(
          env,
          card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR)).obj());
}

void PopulateNativeCreditCardFromJava(
    const jobject& jcard,
    JNIEnv* env,
    CreditCard* card) {
  card->set_origin(
      ConvertJavaStringToUTF8(Java_CreditCard_getOrigin(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_NAME,
      ConvertJavaStringToUTF16(Java_CreditCard_getName(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_NUMBER,
      ConvertJavaStringToUTF16(Java_CreditCard_getNumber(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_EXP_MONTH,
      ConvertJavaStringToUTF16(Java_CreditCard_getMonth(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      ConvertJavaStringToUTF16(Java_CreditCard_getYear(env, jcard)));
}

}  // namespace

PersonalDataManagerAndroid::PersonalDataManagerAndroid(JNIEnv* env,
                                                       jobject obj)
    : weak_java_obj_(env, obj),
      personal_data_manager_(PersonalDataManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile())) {
  personal_data_manager_->AddObserver(this);
}

PersonalDataManagerAndroid::~PersonalDataManagerAndroid() {
  personal_data_manager_->RemoveObserver(this);
}

jint PersonalDataManagerAndroid::GetProfileCount(JNIEnv* unused_env,
                                                 jobject unused_obj) {
  return personal_data_manager_->GetProfiles().size();
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetProfileByIndex(
    JNIEnv* env,
    jobject unused_obj,
    jint index) {
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager_->GetProfiles();
  size_t index_size_t = static_cast<size_t>(index);
  DCHECK_LT(index_size_t, profiles.size());
  return CreateJavaProfileFromNative(env, *profiles[index_size_t]);
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetProfileByGUID(
    JNIEnv* env,
    jobject unused_obj,
    jstring jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (!profile)
    return ScopedJavaLocalRef<jobject>();

  return CreateJavaProfileFromNative(env, *profile);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfile(
    JNIEnv* env,
    jobject unused_obj,
    jobject jprofile) {
  std::string guid = ConvertJavaStringToUTF8(
      env,
      Java_AutofillProfile_getGUID(env, jprofile).obj());

  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  if (guid.empty()) {
    personal_data_manager_->AddProfile(profile);
  } else {
    profile.set_guid(guid);
    personal_data_manager_->UpdateProfile(profile);
  }

  return ConvertUTF8ToJavaString(env, profile.guid());
}

jint PersonalDataManagerAndroid::GetCreditCardCount(JNIEnv* unused_env,
                                                    jobject unused_obj) {
  return personal_data_manager_->GetCreditCards().size();
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardByIndex(
    JNIEnv* env,
    jobject unused_obj,
    jint index) {
  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager_->GetCreditCards();
  size_t index_size_t = static_cast<size_t>(index);
  DCHECK_LT(index_size_t, credit_cards.size());
  return CreateJavaCreditCardFromNative(env, *credit_cards[index_size_t]);
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardByGUID(
    JNIEnv* env,
    jobject unused_obj,
    jstring jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
          ConvertJavaStringToUTF8(env, jguid));
  if (!card)
    return ScopedJavaLocalRef<jobject>();

  return CreateJavaCreditCardFromNative(env, *card);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetCreditCard(
    JNIEnv* env,
    jobject unused_obj,
    jobject jcard) {
  std::string guid = ConvertJavaStringToUTF8(
       env,
       Java_CreditCard_getGUID(env, jcard).obj());

  CreditCard card;
  PopulateNativeCreditCardFromJava(jcard, env, &card);

  if (guid.empty()) {
    personal_data_manager_->AddCreditCard(card);
  } else {
    card.set_guid(guid);
    personal_data_manager_->UpdateCreditCard(card);
  }
  return ConvertUTF8ToJavaString(env, card.guid());
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetProfileLabels(
    JNIEnv* env,
    jobject unused_obj) {
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(
      personal_data_manager_->GetProfiles(),
      NULL,
      NAME_FULL,
      2,
      g_browser_process->GetApplicationLocale(),
      &labels);

  return base::android::ToJavaArrayOfStrings(env, labels);
}

void PersonalDataManagerAndroid::RemoveByGUID(JNIEnv* env,
                                              jobject unused_obj,
                                              jstring jguid) {
  personal_data_manager_->RemoveByGUID(ConvertJavaStringToUTF8(env, jguid));
}

void PersonalDataManagerAndroid::OnPersonalDataChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (weak_java_obj_.get(env).is_null())
    return;

  Java_PersonalDataManager_personalDataChanged(env,
                                               weak_java_obj_.get(env).obj());
}

// static
bool PersonalDataManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Returns whether the Autofill feature is enabled.
static jboolean IsAutofillEnabled(JNIEnv* env, jclass clazz) {
  return GetPrefs()->GetBoolean(autofill::prefs::kAutofillEnabled);
}

// Enables or disables the Autofill feature.
static void SetAutofillEnabled(JNIEnv* env, jclass clazz, jboolean enable) {
  GetPrefs()->SetBoolean(autofill::prefs::kAutofillEnabled, enable);
}

// Returns whether Autofill feature is managed.
static jboolean IsAutofillManaged(JNIEnv* env, jclass clazz) {
  return GetPrefs()->IsManagedPreference(autofill::prefs::kAutofillEnabled);
}

// Returns an ISO 3166-1-alpha-2 country code for a |jcountry_name| using
// the application locale, or an empty string.
static jstring ToCountryCode(JNIEnv* env, jclass clazz, jstring jcountry_name) {
  return ConvertUTF8ToJavaString(
      env,
      AutofillCountry::GetCountryCode(
          base::android::ConvertJavaStringToUTF16(env, jcountry_name),
          g_browser_process->GetApplicationLocale())).Release();
}

static jlong Init(JNIEnv* env, jobject obj) {
  PersonalDataManagerAndroid* personal_data_manager_android =
      new PersonalDataManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(personal_data_manager_android);
}

}  // namespace autofill
