// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Populates default autofill profile from user's own Android contact.
#include "components/autofill/core/browser/android/auxiliary_profiles_android.h"

#include <vector>

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/android/auxiliary_profile_loader_android.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/phone_number.h"

// Generates the autofill profile by accessing the Android
// ContactsContract.Profile API through PersonalAutofillPopulator via JNI.
// The generated profile corresponds to the user's "ME" contact in the
// "People" app. The caller passes a vector of profiles into the constructor
// then initiates a fetch using |GetContactsProfile()| method. This clears
// any existing addresses.

// Randomly generated guid. The Autofillprofile class requires a consistent
// unique guid or else things break.
const char kAndroidMeContactA[] = "9A9E1C06-7A3B-48FA-AA4F-135CA6FC25D9";

// The origin string for all profiles loaded from the Android contacts list.
const char kAndroidContactsOrigin[] = "Android Contacts";

namespace {

// Takes misc. address information strings from Android API and collapses
// into single string for "address line 2"
base::string16 CollapseAddress(const base::string16& post_office_box,
                         const base::string16& neighborhood) {
  std::vector<base::string16> accumulator;
  if (!post_office_box.empty())
    accumulator.push_back(post_office_box);
  if (!neighborhood.empty())
    accumulator.push_back(neighborhood);

  return JoinString(accumulator, base::ASCIIToUTF16(", "));
}

}  // namespace

namespace autofill {

AuxiliaryProfilesAndroid::AuxiliaryProfilesAndroid(
    const AuxiliaryProfileLoaderAndroid& profileLoader,
    const std::string& app_locale)
    : profile_loader_(profileLoader),
      app_locale_(app_locale) {}

AuxiliaryProfilesAndroid::~AuxiliaryProfilesAndroid() {
}

scoped_ptr<AutofillProfile> AuxiliaryProfilesAndroid::LoadContactsProfile() {
  scoped_ptr<AutofillProfile> profile(
      new AutofillProfile(kAndroidMeContactA, kAndroidContactsOrigin));
  LoadName(profile.get());
  LoadEmailAddress(profile.get());
  LoadPhoneNumbers(profile.get());

  // Android user's profile contact does not parse its address
  // into constituent parts. Instead we just get a long string blob.
  // Disable address population until we implement a way to parse the
  // data.
  // http://crbug.com/178838
  // LoadAddress(profile.get());

  return profile.Pass();
}

void AuxiliaryProfilesAndroid::LoadAddress(AutofillProfile* profile) {
  base::string16 street = profile_loader_.GetStreet();
  base::string16 post_office_box = profile_loader_.GetPostOfficeBox();
  base::string16 neighborhood = profile_loader_.GetNeighborhood();
  base::string16 city = profile_loader_.GetCity();
  base::string16 postal_code = profile_loader_.GetPostalCode();
  base::string16 region = profile_loader_.GetRegion();
  base::string16 country = profile_loader_.GetCountry();

  base::string16 street2 = CollapseAddress(post_office_box, neighborhood);

  profile->SetRawInfo(ADDRESS_HOME_LINE1, street);
  profile->SetRawInfo(ADDRESS_HOME_LINE2, street2);
  profile->SetRawInfo(ADDRESS_HOME_CITY, city);
  profile->SetRawInfo(ADDRESS_HOME_STATE, region);
  profile->SetRawInfo(ADDRESS_HOME_ZIP, postal_code);
  profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), country, app_locale_);
}

void AuxiliaryProfilesAndroid::LoadName(AutofillProfile* profile) {
  base::string16 first_name = profile_loader_.GetFirstName();
  base::string16 middle_name = profile_loader_.GetMiddleName();
  base::string16 last_name = profile_loader_.GetLastName();

  profile->SetRawInfo(NAME_FIRST, first_name);
  profile->SetRawInfo(NAME_MIDDLE, middle_name);
  profile->SetRawInfo(NAME_LAST, last_name);
}

void AuxiliaryProfilesAndroid::LoadEmailAddress(AutofillProfile* profile) {
  std::vector<base::string16> emails;
  profile_loader_.GetEmailAddresses(&emails);
  profile->SetRawMultiInfo(EMAIL_ADDRESS, emails);
}

void AuxiliaryProfilesAndroid::LoadPhoneNumbers(AutofillProfile* profile) {
  std::vector<base::string16> phone_numbers;
  profile_loader_.GetPhoneNumbers(&phone_numbers);
  profile->SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, phone_numbers);
}

} // namespace autofill
