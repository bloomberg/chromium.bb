// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/android/auxiliary_profile_loader_android.h"

namespace autofill {

class TestAuxiliaryProfileLoader
    : public autofill::AuxiliaryProfileLoaderAndroid {
  // Mock object for unit testing |AuxiliaryProfilesAndroid|
 public:
  TestAuxiliaryProfileLoader();
  virtual ~TestAuxiliaryProfileLoader();

  virtual bool GetHasPermissions() const OVERRIDE;

  virtual base::string16 GetFirstName() const OVERRIDE;
  virtual base::string16 GetMiddleName() const OVERRIDE;
  virtual base::string16 GetLastName() const OVERRIDE;
  virtual base::string16 GetSuffix() const OVERRIDE;

  virtual base::string16 GetStreet() const OVERRIDE;
  virtual base::string16 GetCity() const OVERRIDE;
  virtual base::string16 GetNeighborhood() const OVERRIDE;
  virtual base::string16 GetPostalCode() const OVERRIDE;
  virtual base::string16 GetRegion() const OVERRIDE;
  virtual base::string16 GetPostOfficeBox() const OVERRIDE;
  virtual base::string16 GetCountry() const OVERRIDE;

  virtual void GetEmailAddresses(
      std::vector<base::string16>* email_addresses) const OVERRIDE;
  virtual void GetPhoneNumbers(
      std::vector<base::string16>* phone_numbers) const OVERRIDE;

  void SetFirstName(const base::string16& first_name);
  void SetMiddleName(const base::string16& middle_name);
  void SetLastName(const base::string16& last_name);
  void SetSuffix(const base::string16& suffix);

  void SetStreet(const base::string16& street);
  void SetPostOfficeBox(const base::string16& post_office_box);
  void SetNeighborhood(const base::string16& neighborhood);
  void SetRegion(const base::string16& region);
  void SetCity(const base::string16& city);
  void SetPostalCode(const base::string16& postal_code);
  void SetCountry(const base::string16& country);

  void SetEmailAddresses(const std::vector<base::string16>& email_addresses);
  void SetPhoneNumbers(const std::vector<base::string16>& phone_numbers);

 private:
  base::string16 street_;
  base::string16 post_office_box_;
  base::string16 neighborhood_;
  base::string16 region_;
  base::string16 city_;
  base::string16 postal_code_;
  base::string16 country_;
  base::string16 first_name_;
  base::string16 middle_name_;
  base::string16 last_name_;
  base::string16 suffix_;
  std::vector<base::string16> email_addresses_;
  std::vector<base::string16> phone_numbers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_
