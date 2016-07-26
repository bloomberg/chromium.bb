// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/personal_data_manager_android.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_scaled_resources.h"
#include "jni/PersonalDataManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {
namespace {

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

ScopedJavaLocalRef<jobject> CreateJavaProfileFromNative(
    JNIEnv* env,
    const AutofillProfile& profile) {
  return Java_AutofillProfile_create(
      env, ConvertUTF8ToJavaString(env, profile.guid()).obj(),
      ConvertUTF8ToJavaString(env, profile.origin()).obj(),
      profile.record_type() == AutofillProfile::LOCAL_PROFILE,
      ConvertUTF16ToJavaString(env, profile.GetInfo(
          AutofillType(NAME_FULL),
          g_browser_process->GetApplicationLocale())).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(COMPANY_NAME)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_STATE)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_CITY)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_ZIP)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_COUNTRY)).obj(),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER)).obj(),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(EMAIL_ADDRESS)).obj(),
      ConvertUTF8ToJavaString(env, profile.language_code()).obj());
}

void MaybeSetRawInfo(AutofillProfile* profile,
                     autofill::ServerFieldType type,
                     const base::android::JavaRef<jstring>& jstr) {
  if (!jstr.is_null())
    profile->SetRawInfo(type, ConvertJavaStringToUTF16(jstr));
}

void PopulateNativeProfileFromJava(
    const JavaParamRef<jobject>& jprofile,
    JNIEnv* env,
    AutofillProfile* profile) {
  profile->set_origin(
      ConvertJavaStringToUTF8(
          Java_AutofillProfile_getOrigin(env, jprofile.obj())));
  profile->SetInfo(AutofillType(NAME_FULL),
                   ConvertJavaStringToUTF16(
                       Java_AutofillProfile_getFullName(env, jprofile.obj())),
                   g_browser_process->GetApplicationLocale());
  MaybeSetRawInfo(profile, autofill::COMPANY_NAME,
                  Java_AutofillProfile_getCompanyName(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_STREET_ADDRESS,
                  Java_AutofillProfile_getStreetAddress(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_STATE,
                  Java_AutofillProfile_getRegion(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_CITY,
                  Java_AutofillProfile_getLocality(env, jprofile.obj()));
  MaybeSetRawInfo(
      profile, autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      Java_AutofillProfile_getDependentLocality(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_ZIP,
                  Java_AutofillProfile_getPostalCode(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_SORTING_CODE,
                  Java_AutofillProfile_getSortingCode(env, jprofile.obj()));
  ScopedJavaLocalRef<jstring> country_code =
      Java_AutofillProfile_getCountryCode(env, jprofile.obj());
  if (!country_code.is_null()) {
    profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                     ConvertJavaStringToUTF16(country_code),
                     g_browser_process->GetApplicationLocale());
  }
  MaybeSetRawInfo(profile, autofill::PHONE_HOME_WHOLE_NUMBER,
                  Java_AutofillProfile_getPhoneNumber(env, jprofile.obj()));
  MaybeSetRawInfo(profile, autofill::EMAIL_ADDRESS,
                  Java_AutofillProfile_getEmailAddress(env, jprofile.obj()));
  profile->set_language_code(
      ConvertJavaStringToUTF8(
          Java_AutofillProfile_getLanguageCode(env, jprofile.obj())));
}

// Mapping from Chrome card types to PaymentRequest basic card payment spec and
// icons. Note that "generic" is not in the spec.
// https://w3c.github.io/webpayments-methods-card/#method-id
const struct PaymentRequestData {
  const char* card_type;
  const char* basic_card_payment_type;
  const int icon_resource_id;
} kPaymentRequestData[] {
  {"genericCC", "generic", IDR_AUTOFILL_PR_GENERIC},

  {"americanExpressCC", "amex", IDR_AUTOFILL_PR_AMEX},
  {"dinersCC", "diners", IDR_AUTOFILL_PR_DINERS},
  {"discoverCC", "discover", IDR_AUTOFILL_PR_DISCOVER},
  {"jcbCC", "jcb", IDR_AUTOFILL_PR_JCB},
  {"masterCardCC", "mastercard", IDR_AUTOFILL_PR_MASTERCARD},
  {"unionPayCC", "unionpay", IDR_AUTOFILL_PR_UNIONPAY},
  {"visaCC", "visa", IDR_AUTOFILL_PR_VISA},
};

// Converts the card type into PaymentRequest type according to the basic card
// payment spec and an icon. Will set the type and the icon to "generic" for
// unrecognized card type.
const PaymentRequestData& GetPaymentRequestData(const std::string& type) {
  for (size_t i = 0; i < arraysize(kPaymentRequestData); ++i) {
    if (type == kPaymentRequestData[i].card_type)
      return kPaymentRequestData[i];
  }
  return kPaymentRequestData[0];
}

ScopedJavaLocalRef<jobject> CreateJavaCreditCardFromNative(
    JNIEnv* env,
    const CreditCard& card) {
  const PaymentRequestData& payment_request_data =
      GetPaymentRequestData(card.type());
  return Java_CreditCard_create(
      env, ConvertUTF8ToJavaString(env, card.guid()).obj(),
      ConvertUTF8ToJavaString(env, card.origin()).obj(),
      card.record_type() == CreditCard::LOCAL_CARD,
      card.record_type() == CreditCard::FULL_SERVER_CARD,
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .obj(),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NUMBER)).obj(),
      ConvertUTF16ToJavaString(env, card.TypeAndLastFourDigits()).obj(),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_EXP_MONTH))
          .obj(),
      ConvertUTF16ToJavaString(env,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))
          .obj(),
      ConvertUTF8ToJavaString(env, payment_request_data.basic_card_payment_type)
          .obj(),
      ResourceMapper::MapFromChromiumId(payment_request_data.icon_resource_id),
      ConvertUTF8ToJavaString(env, card.billing_address_id()) .obj());
}

void PopulateNativeCreditCardFromJava(
    const jobject& jcard,
    JNIEnv* env,
    CreditCard* card) {
  card->set_origin(
      ConvertJavaStringToUTF8(Java_CreditCard_getOrigin(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_NAME_FULL,
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
  card->set_billing_address_id(
      ConvertJavaStringToUTF8(Java_CreditCard_getBillingAddressId(env, jcard)));
}

// Self-deleting requester of full card details, including full PAN and the CVC
// number.
class FullCardRequester : public payments::FullCardRequest::Delegate,
                          public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester() {}

  // Takes ownership of |card|.
  void GetFullCard(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jweb_contents,
                   const base::android::JavaParamRef<jobject>& jdelegate,
                   std::unique_ptr<CreditCard> card) {
    card_ = std::move(card);
    GetFullCard(env, jweb_contents, jdelegate, card_.get());
  }

  // Does not take ownership of |card|.
  void GetFullCard(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jweb_contents,
                   const base::android::JavaParamRef<jobject>& jdelegate,
                   const CreditCard* card) {
    jdelegate_.Reset(env, jdelegate);

    if (!card) {
      OnFullCardError();
      return;
    }

    content::WebContents* contents =
        content::WebContents::FromJavaWebContents(jweb_contents);
    if (!contents) {
      OnFullCardError();
      return;
    }

    ContentAutofillDriverFactory* factory =
        ContentAutofillDriverFactory::FromWebContents(contents);
    if (!factory) {
      OnFullCardError();
      return;
    }

    ContentAutofillDriver* driver =
        factory->DriverForFrame(contents->GetMainFrame());
    if (!driver) {
      OnFullCardError();
      return;
    }

    driver->autofill_manager()->GetOrCreateFullCardRequest()->GetFullCard(
        *card, AutofillClient::UNMASK_FOR_PAYMENT_REQUEST, AsWeakPtr());
  }

 private:
  virtual ~FullCardRequester() {}

  // payments::FullCardRequest::Delegate:
  void OnFullCardDetails(const CreditCard& card,
                         const base::string16& cvc) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FullCardRequestDelegate_onFullCardDetails(
        env, jdelegate_.obj(), CreateJavaCreditCardFromNative(env, card).obj(),
        base::android::ConvertUTF16ToJavaString(env, cvc).obj());
    delete this;
  }

  // payments::FullCardRequest::Delegate:
  void OnFullCardError() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FullCardRequestDelegate_onFullCardError(env, jdelegate_.obj());
    delete this;
  }

  std::unique_ptr<CreditCard> card_;
  ScopedJavaGlobalRef<jobject> jdelegate_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequester);
};

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

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileGUIDsForSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileGUIDs(env, personal_data_manager_->GetProfiles());
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileGUIDsToSuggest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileGUIDs(env, personal_data_manager_->GetProfilesToSuggest());
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetProfileByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (!profile)
    return ScopedJavaLocalRef<jobject>();

  return CreateJavaProfileFromNative(env, *profile);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile) {
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

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileLabelsForSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileLabels(env, false, personal_data_manager_->GetProfiles());
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileLabelsToSuggest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileLabels(env, true,
                          personal_data_manager_->GetProfilesToSuggest());
}

base::android::ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetAddressLabelForPaymentRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  std::vector<ServerFieldType> label_fields;
  label_fields.push_back(COMPANY_NAME);
  label_fields.push_back(ADDRESS_HOME_LINE1);
  label_fields.push_back(ADDRESS_HOME_LINE2);
  label_fields.push_back(ADDRESS_HOME_DEPENDENT_LOCALITY);
  label_fields.push_back(ADDRESS_HOME_CITY);
  label_fields.push_back(ADDRESS_HOME_STATE);
  label_fields.push_back(ADDRESS_HOME_ZIP);
  label_fields.push_back(ADDRESS_HOME_SORTING_CODE);
  label_fields.push_back(ADDRESS_HOME_COUNTRY);

  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  return ConvertUTF16ToJavaString(
      env, profile.ConstructInferredLabel(
               label_fields, label_fields.size(),
               g_browser_process->GetApplicationLocale()));
}

base::android::ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetCreditCardGUIDsForSettings(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return GetCreditCardGUIDs(env, personal_data_manager_->GetCreditCards());
}

base::android::ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetCreditCardGUIDsToSuggest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return GetCreditCardGUIDs(env,
                            personal_data_manager_->GetCreditCardsToSuggest());
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
          ConvertJavaStringToUTF8(env, jguid));
  if (!card)
    return ScopedJavaLocalRef<jobject>();

  return CreateJavaCreditCardFromNative(env, *card);
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardForNumber(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jcard_number) {
  // A local card with empty GUID.
  CreditCard card("", "");
  card.SetNumber(ConvertJavaStringToUTF16(env, jcard_number));
  return CreateJavaCreditCardFromNative(env, card);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetCreditCard(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jcard) {
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


void PersonalDataManagerAndroid::UpdateServerCardBillingAddress(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid,
    const JavaParamRef<jstring>& jbilling_address_id) {
  CreditCard card(ConvertJavaStringToUTF8(env, jguid), kSettingsOrigin);
  card.set_record_type(CreditCard::MASKED_SERVER_CARD);
  card.set_billing_address_id(ConvertJavaStringToUTF8(env,
      jbilling_address_id));
  personal_data_manager_->UpdateServerCardBillingAddress(card);
}

ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetBasicCardPaymentTypeIfValid(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jcard_number) {
  base::string16 card_number = ConvertJavaStringToUTF16(env, jcard_number);
  return ConvertUTF8ToJavaString(
      env,
      IsValidCreditCardNumber(card_number)
          ? GetPaymentRequestData(CreditCard::GetCreditCardType(card_number))
                .basic_card_payment_type
          : "");
}

void PersonalDataManagerAndroid::AddServerCreditCardForTest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jcard) {
  std::unique_ptr<CreditCard> card(new CreditCard);
  PopulateNativeCreditCardFromJava(jcard, env, card.get());
  card->set_record_type(CreditCard::MASKED_SERVER_CARD);
  personal_data_manager_->AddServerCreditCardForTest(std::move(card));
  personal_data_manager_->NotifyPersonalDataChangedForTest();
}

void PersonalDataManagerAndroid::RemoveByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  personal_data_manager_->RemoveByGUID(ConvertJavaStringToUTF8(env, jguid));
}

void PersonalDataManagerAndroid::ClearUnmaskedCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& guid) {
  personal_data_manager_->ResetFullServerCard(
      ConvertJavaStringToUTF8(env, guid));
}

void PersonalDataManagerAndroid::GetFullCardForPaymentRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jguid,
    const JavaParamRef<jobject>& jdelegate) {
  // Self-deleting object that does not take ownership of the CreditCard from
  // the PersonalDataManager. The PersonalDataManager owns that CreditCard.
  (new FullCardRequester())
      ->GetFullCard(env, jweb_contents, jdelegate,
                    personal_data_manager_->GetCreditCardByGUID(
                        ConvertJavaStringToUTF8(env, jguid)));
}

void PersonalDataManagerAndroid::GetFullTemporaryCardForPaymentRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jcard_number,
    const JavaParamRef<jstring>& jname_on_card,
    const JavaParamRef<jstring>& jexpiration_month,
    const JavaParamRef<jstring>& jexpiration_year,
    const JavaParamRef<jobject>& jdelegate) {
  // FullCardRequest will not attempt to save a card with an empty GUID.
  std::unique_ptr<CreditCard> card(new CreditCard("", ""));
  card->SetNumber(ConvertJavaStringToUTF16(env, jcard_number));
  card->SetRawInfo(
      CREDIT_CARD_NAME_FULL,
      ConvertJavaStringToUTF16(env, jname_on_card));
  card->SetRawInfo(
      CREDIT_CARD_EXP_MONTH,
      ConvertJavaStringToUTF16(env, jexpiration_month));
  card->SetRawInfo(
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      ConvertJavaStringToUTF16(env, jexpiration_year));

  // Self-deleting object that takes ownership of the CreditCard.
  (new FullCardRequester())
      ->GetFullCard(env, jweb_contents, jdelegate, std::move(card));
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

void PersonalDataManagerAndroid::RecordAndLogProfileUse(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (profile)
    personal_data_manager_->RecordUseOf(*profile);
}

void PersonalDataManagerAndroid::SetProfileUseStatsForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid,
    jint count,
    jint date) {
  DCHECK(count >= 0 && date >= 0);

  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  profile->set_use_count(static_cast<size_t>(count));
  profile->set_use_date(base::Time::FromTimeT(date));
  personal_data_manager_->NotifyPersonalDataChangedForTest();
}

jint PersonalDataManagerAndroid::GetProfileUseCountForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return profile->use_count();
}

jlong PersonalDataManagerAndroid::GetProfileUseDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return profile->use_date().ToTimeT();
}

void PersonalDataManagerAndroid::RecordAndLogCreditCardUse(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (card)
    personal_data_manager_->RecordUseOf(*card);
}

void PersonalDataManagerAndroid::SetCreditCardUseStatsForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid,
    jint count,
    jint date) {
  DCHECK(count >= 0 && date >= 0);

  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  card->set_use_count(static_cast<size_t>(count));
  card->set_use_date(base::Time::FromTimeT(date));
  personal_data_manager_->NotifyPersonalDataChangedForTest();
}

jint PersonalDataManagerAndroid::GetCreditCardUseCountForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return card->use_count();
}

jlong PersonalDataManagerAndroid::GetCreditCardUseDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid){
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return card->use_date().ToTimeT();
}

// TODO(crbug.com/629507): Use a mock clock for testing.
jlong PersonalDataManagerAndroid::GetCurrentDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return base::Time::Now().ToTimeT();
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetProfileGUIDs(
    JNIEnv* env,
    const std::vector<AutofillProfile*>& profiles) {
  std::vector<base::string16> guids;
  for (AutofillProfile* profile : profiles)
    guids.push_back(base::UTF8ToUTF16(profile->guid()));

  return base::android::ToJavaArrayOfStrings(env, guids);
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetCreditCardGUIDs(
    JNIEnv* env,
    const std::vector<CreditCard*>& credit_cards) {
  std::vector<base::string16> guids;
  for (CreditCard* credit_card : credit_cards)
    guids.push_back(base::UTF8ToUTF16(credit_card->guid()));

  return base::android::ToJavaArrayOfStrings(env, guids);
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetProfileLabels(
    JNIEnv* env,
    bool address_only,
    std::vector<AutofillProfile*> profiles) {
  std::unique_ptr<std::vector<ServerFieldType>> suggested_fields;
  size_t minimal_fields_shown = 2;
  if (address_only) {
    suggested_fields.reset(new std::vector<ServerFieldType>);
    suggested_fields->push_back(COMPANY_NAME);
    suggested_fields->push_back(ADDRESS_HOME_LINE1);
    suggested_fields->push_back(ADDRESS_HOME_LINE2);
    suggested_fields->push_back(ADDRESS_HOME_DEPENDENT_LOCALITY);
    suggested_fields->push_back(ADDRESS_HOME_CITY);
    suggested_fields->push_back(ADDRESS_HOME_STATE);
    suggested_fields->push_back(ADDRESS_HOME_ZIP);
    suggested_fields->push_back(ADDRESS_HOME_SORTING_CODE);
    suggested_fields->push_back(ADDRESS_HOME_COUNTRY);
    minimal_fields_shown = suggested_fields->size();
  }

  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(
      profiles, suggested_fields.get(), NAME_FULL, minimal_fields_shown,
      g_browser_process->GetApplicationLocale(), &labels);

  return base::android::ToJavaArrayOfStrings(env, labels);
}

// Returns whether the Autofill feature is enabled.
static jboolean IsAutofillEnabled(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz) {
  return GetPrefs()->GetBoolean(autofill::prefs::kAutofillEnabled);
}

// Enables or disables the Autofill feature.
static void SetAutofillEnabled(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz,
                               jboolean enable) {
  GetPrefs()->SetBoolean(autofill::prefs::kAutofillEnabled, enable);
}

// Returns whether the Autofill feature is managed.
static jboolean IsAutofillManaged(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz) {
  return GetPrefs()->IsManagedPreference(autofill::prefs::kAutofillEnabled);
}

// Returns whether the Payments integration feature is enabled.
static jboolean IsPaymentsIntegrationEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return GetPrefs()->GetBoolean(autofill::prefs::kAutofillWalletImportEnabled);
}

// Enables or disables the Payments integration feature.
static void SetPaymentsIntegrationEnabled(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          jboolean enable) {
  GetPrefs()->SetBoolean(autofill::prefs::kAutofillWalletImportEnabled, enable);
}

// Returns an ISO 3166-1-alpha-2 country code for a |jcountry_name| using
// the application locale, or an empty string.
static ScopedJavaLocalRef<jstring> ToCountryCode(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jcountry_name) {
  return ConvertUTF8ToJavaString(
      env, CountryNames::GetInstance()->GetCountryCode(
               base::android::ConvertJavaStringToUTF16(env, jcountry_name)));
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PersonalDataManagerAndroid* personal_data_manager_android =
      new PersonalDataManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(personal_data_manager_android);
}

}  // namespace autofill
