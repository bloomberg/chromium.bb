// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILE_LOADER_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILE_LOADER_ANDROID_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/strings/string16.h"

namespace autofill {

bool RegisterAuxiliaryProfileLoader(JNIEnv* env);

// This class loads user's contact information from their device.
// The populated data corresponds to the user's 'Me' profile in their
// contact list.
class AuxiliaryProfileLoaderAndroid {
 public:
  AuxiliaryProfileLoaderAndroid();
  virtual ~AuxiliaryProfileLoaderAndroid();

  // Init method should be called after object initialization.
  // context parameter is an Android context.
  void Init(JNIEnv* env, const jobject& context);

  // Returns true IFF the application has priviledges to access the user's
  // contact information.
  virtual bool GetHasPermissions() const;
  // Returns address street.
  virtual base::string16 GetStreet() const;
  // Returns address post office box.
  virtual base::string16 GetPostOfficeBox() const;
  // Returns address neighborhood (e.g. Noe Valley, Nob Hill, Twin Peaks, ...).
  virtual base::string16 GetNeighborhood() const;
  // Returns address region such as state or province information
  // (e.g. Ontario, California, Hubei).
  virtual base::string16 GetRegion() const;
  // Returns address city.
  virtual base::string16 GetCity() const;
  // Returns address postal code or zip code.
  virtual base::string16 GetPostalCode() const;
  // Returns address country.
  virtual base::string16 GetCountry() const;

  // Returns contact's first name.
  virtual base::string16 GetFirstName() const;
  // Returns contact's middle name.
  virtual base::string16 GetMiddleName() const;
  // Returns contact's last name.
  virtual base::string16 GetLastName() const;
  // Returns contact's suffix (e.g. Ph.D, M.D., ...).
  virtual base::string16 GetSuffix() const;

  // Populates string vector parameter with contact's email addresses.
  virtual void GetEmailAddresses(
      std::vector<base::string16>* email_addresses) const;

  // Populates string vector parameter with contact's phones numbers.
  virtual void GetPhoneNumbers(
      std::vector<base::string16>* phone_numbers) const;

 private:
  JNIEnv* env_;
  // The reference to java |PersonalAutofillPopulator| which
  // actually extracts users contact information from the physical device
  base::android::ScopedJavaLocalRef<jobject> populator_;
  DISALLOW_COPY_AND_ASSIGN(AuxiliaryProfileLoaderAndroid);
};

} // namespace

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_AUXILIARY_PROFILE_LOADER_ANDROID_H_
