// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "jni/AutofillProfileBridge_jni.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;

namespace autofill {

namespace {

// Address field types. This list must be kept in-sync with the corresponding
// AddressField class in AutofillProfileBridge.java.
enum AddressField {
  COUNTRY,             // Country code.
  ADMIN_AREA,          // Administrative area such as a state, province,
                       // island, etc.
  LOCALITY,            // City or locality.
  DEPENDENT_LOCALITY,  // Dependent locality (may be an inner-city district or
                       // a suburb).
  SORTING_CODE,        // Sorting code.
  POSTAL_CODE,         // Zip or postal code.
  STREET_ADDRESS,      // Street address lines.
  ORGANIZATION,        // Organization, company, firm, institution, etc.
  RECIPIENT            // Name.
};

} // namespace

static ScopedJavaLocalRef<jstring> GetDefaultCountryCode(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  std::string default_country_code =
      autofill::AutofillCountry::CountryCodeForLocale(
          g_browser_process->GetApplicationLocale());
  return ConvertUTF8ToJavaString(env, default_country_code);
}

static void GetSupportedCountries(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& j_country_code_list,
    const JavaParamRef<jobject>& j_country_name_list) {
  std::vector<std::string> country_codes =
      ::i18n::addressinput::GetRegionCodes();
  std::vector<std::string> known_country_codes;
  std::vector<base::string16> known_country_names;
  std::string locale = g_browser_process->GetApplicationLocale();
  for (auto country_code : country_codes) {
    const base::string16& country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    // Don't display a country code for which a name is not known yet.
    if (country_name != base::UTF8ToUTF16(country_code)) {
      known_country_codes.push_back(country_code);
      known_country_names.push_back(country_name);
    }
  }

  Java_AutofillProfileBridge_stringArrayToList(
      env, ToJavaArrayOfStrings(env, known_country_codes).obj(),
      j_country_code_list);
  Java_AutofillProfileBridge_stringArrayToList(
      env, ToJavaArrayOfStrings(env, known_country_names).obj(),
      j_country_name_list);
}

static ScopedJavaLocalRef<jstring> GetAddressUiComponents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_country_code,
    const JavaParamRef<jstring>& j_language_code,
    const JavaParamRef<jobject>& j_id_list,
    const JavaParamRef<jobject>& j_name_list) {
  std::string best_language_tag;
  std::vector<int> component_ids;
  std::vector<std::string> component_labels;
  ::i18n::addressinput::Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);

  std::string language_code;
  if (j_language_code != NULL) {
    language_code = ConvertJavaStringToUTF8(env, j_language_code);
  }
  if (language_code.empty()) {
    language_code = g_browser_process->GetApplicationLocale();
  }

  std::vector<::i18n::addressinput::AddressUiComponent> ui_components =
      ::i18n::addressinput::BuildComponents(
          ConvertJavaStringToUTF8(env, j_country_code), localization,
          language_code, &best_language_tag);

  for (auto ui_component : ui_components) {
    component_labels.push_back(ui_component.name);

    switch (ui_component.field) {
          case ::i18n::addressinput::COUNTRY:
            component_ids.push_back(AddressField::COUNTRY);
            break;
          case ::i18n::addressinput::ADMIN_AREA:
            component_ids.push_back(AddressField::ADMIN_AREA);
            break;
          case ::i18n::addressinput::LOCALITY:
            component_ids.push_back(AddressField::LOCALITY);
            break;
          case ::i18n::addressinput::DEPENDENT_LOCALITY:
            component_ids.push_back(AddressField::DEPENDENT_LOCALITY);
            break;
          case ::i18n::addressinput::SORTING_CODE:
            component_ids.push_back(AddressField::SORTING_CODE);
            break;
          case ::i18n::addressinput::POSTAL_CODE:
            component_ids.push_back(AddressField::POSTAL_CODE);
            break;
          case ::i18n::addressinput::STREET_ADDRESS:
            component_ids.push_back(AddressField::STREET_ADDRESS);
            break;
          case ::i18n::addressinput::ORGANIZATION:
            component_ids.push_back(AddressField::ORGANIZATION);
            break;
          case ::i18n::addressinput::RECIPIENT:
            component_ids.push_back(AddressField::RECIPIENT);
            break;
          default:
            NOTREACHED();
    }
  }

  Java_AutofillProfileBridge_intArrayToList(env,
                                            ToJavaIntArray(
                                                   env, component_ids).obj(),
                                               j_id_list);
  Java_AutofillProfileBridge_stringArrayToList(env,
                                               ToJavaArrayOfStrings(
                                                   env, component_labels).obj(),
                                               j_name_list);

  return ConvertUTF8ToJavaString(env, best_language_tag);
}

// static
bool RegisterAutofillProfileBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace autofill
