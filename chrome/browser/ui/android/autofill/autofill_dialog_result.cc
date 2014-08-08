// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/common/form_data.h"
#include "jni/AutofillDialogResult_jni.h"

namespace autofill {

namespace {

std::string ConvertNullOrJavaStringToUTF8(JNIEnv* env, jstring str) {
  return str ? base::android::ConvertJavaStringToUTF8(env, str) : std::string();
}

base::string16 ConvertNullOrJavaStringToUTF16(JNIEnv* env, jstring str) {
  return str ? base::android::ConvertJavaStringToUTF16(env, str)
             : base::string16();
}

#define FETCH_JFIELD(env, jobj, cls, getter) \
    (Java_##cls##_get##getter((env), (jobj)))

#define FETCH_JSTRING(utf, env, jobj, cls, getter) \
    (ConvertNullOrJavaStringTo##utf( \
        (env), FETCH_JFIELD((env), (jobj), cls, getter).obj()))

scoped_ptr<wallet::Address> ParseJavaWalletAddress(
    JNIEnv* env, jobject address) {
  if (!address)
    return scoped_ptr<wallet::Address>();

  const base::string16 recipient_name =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, Name);

  std::vector<base::string16> address_lines;
  const base::string16 street_address =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, StreetAddress);
  base::SplitString(street_address, base::char16('\n'), &address_lines);

  const base::string16 locality_name =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, Locality);
  const base::string16 dependent_locality_name =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, DependentLocality);
  const base::string16 administrative_area_name =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, AdministrativeArea);
  const base::string16 postal_code_number =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, PostalCode);
  const base::string16 sorting_code =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, SortingCode);
  const base::string16 phone_number =
      FETCH_JSTRING(UTF16, env, address, ResultAddress, PhoneNumber);
  const std::string country_name_code =
      FETCH_JSTRING(UTF8, env, address, ResultAddress, CountryCode);
  DCHECK(!country_name_code.empty());
  const std::string language_code =
      FETCH_JSTRING(UTF8, env, address, ResultAddress, LanguageCode);

  return scoped_ptr<wallet::Address>(new wallet::Address(
      country_name_code,
      recipient_name,
      address_lines,
      locality_name,
      dependent_locality_name,
      administrative_area_name,
      postal_code_number,
      sorting_code,
      phone_number,
      std::string(),
      language_code));
}

scoped_ptr<wallet::FullWallet> ParseJavaWallet(JNIEnv* env, jobject wallet) {
  const ScopedJavaLocalRef<jobject> billing_address(
      FETCH_JFIELD(env, wallet, ResultWallet, BillingAddress));
  const ScopedJavaLocalRef<jobject> shipping_address(
      FETCH_JFIELD(env, wallet, ResultWallet, ShippingAddress));
  const ScopedJavaLocalRef<jobject> card(
      FETCH_JFIELD(env, wallet, ResultWallet, Card));

  const int expiration_month =
      FETCH_JFIELD(env, card.obj(), ResultCard, ExpirationMonth);
  const int expiration_year =
      FETCH_JFIELD(env, card.obj(), ResultCard, ExpirationYear);
  const std::string pan =
      FETCH_JSTRING(UTF8, env, card.obj(), ResultCard, Pan);
  const std::string cvn =
      FETCH_JSTRING(UTF8, env, card.obj(), ResultCard, Cvn);

  return wallet::FullWallet::CreateFullWalletFromClearText(
      expiration_month,
      expiration_year,
      pan,
      cvn,
      ParseJavaWalletAddress(env, billing_address.obj()),
      ParseJavaWalletAddress(env, shipping_address.obj()));
}

base::string16 ParseWalletEmail(JNIEnv* env, jobject wallet) {
  return FETCH_JSTRING(UTF16, env, wallet, ResultWallet, Email);
}

std::string ParseGoogleTransactionId(JNIEnv* env, jobject wallet) {
  return FETCH_JSTRING(UTF8, env, wallet, ResultWallet, GoogleTransactionId);
}

#undef FETCH_JSTRING
#undef FETCH_FIELD

}  // namespace

// static
scoped_ptr<wallet::FullWallet> AutofillDialogResult::ConvertFromJava(
    JNIEnv* env, jobject wallet) {
  return ParseJavaWallet(env, wallet);
}

// static
base::string16 AutofillDialogResult::GetWalletEmail(
    JNIEnv* env, jobject wallet) {
  return ParseWalletEmail(env, wallet);
}

// static
std::string AutofillDialogResult::GetWalletGoogleTransactionId(
    JNIEnv* env, jobject wallet) {
  return ParseGoogleTransactionId(env, wallet);
}

// static
bool AutofillDialogResult::RegisterAutofillDialogResult(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
