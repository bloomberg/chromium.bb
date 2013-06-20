// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILES_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILES_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/android/auxiliary_profile_loader_android.h"

namespace autofill {

class AutofillProfile;
class AuxiliaryProfileLoaderAndroid;

 // This class is used to populate an AutofillProfile vector with
 // a 'default' auxiliary profile. It depends on an
 // AuxiliaryProfileLoaderAndroid object to provide contact information
 // that is re-organized into an Autofill profile.
class AuxiliaryProfilesAndroid {
 public:
  // Takes in an AuxiliaryProfileLoader object which provides contact
  // information methods.
  AuxiliaryProfilesAndroid(
     const autofill::AuxiliaryProfileLoaderAndroid& profile_loader,
     const std::string& app_locale);
  ~AuxiliaryProfilesAndroid();

  // Returns autofill profile constructed from profile_loader_.
  scoped_ptr<AutofillProfile> LoadContactsProfile();

 private:
  // Inserts contact's address data into profile.
  void LoadAddress(AutofillProfile* profile);
  // Inserts contact's name data into profile.
  void LoadName(AutofillProfile* profile);
  // Inserts contact's email address data into profile.
  void LoadEmailAddress(AutofillProfile* profile);
  // Inserts contact's phone number data into profile.
  void LoadPhoneNumbers(AutofillProfile* profile);

  const AuxiliaryProfileLoaderAndroid& profile_loader_;
  std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(AuxiliaryProfilesAndroid);
};

} // namespace

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILES_ANDROID_H_
