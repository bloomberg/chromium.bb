// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_
#define COMPONENTS_AUTOFILL_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_

#include "base/compiler_specific.h"
#include "components/autofill/browser/android/auxiliary_profile_loader_android.h"

class TestAuxiliaryProfileLoader
    : public autofill::AuxiliaryProfileLoaderAndroid {
  // Mock object for unit testing |AuxiliaryProfilesAndroid|
 public:
  TestAuxiliaryProfileLoader();
  virtual ~TestAuxiliaryProfileLoader();

  virtual bool GetHasPermissions() const OVERRIDE;

  virtual string16 GetFirstName() const OVERRIDE;
  virtual string16 GetMiddleName() const OVERRIDE;
  virtual string16 GetLastName() const OVERRIDE;
  virtual string16 GetSuffix() const OVERRIDE;

  virtual string16 GetStreet() const OVERRIDE;
  virtual string16 GetCity() const OVERRIDE;
  virtual string16 GetNeighborhood() const OVERRIDE;
  virtual string16 GetPostalCode() const OVERRIDE;
  virtual string16 GetRegion() const OVERRIDE;
  virtual string16 GetPostOfficeBox() const OVERRIDE;
  virtual string16 GetCountry() const OVERRIDE;

  virtual void GetEmailAddresses(
      std::vector<string16>* email_addresses) const OVERRIDE;
  virtual void GetPhoneNumbers(
      std::vector<string16>* phone_numbers) const OVERRIDE;

  void SetFirstName(const string16& first_name);
  void SetMiddleName(const string16& middle_name);
  void SetLastName(const string16& last_name);
  void SetSuffix(const string16& suffix);

  void SetStreet(const string16& street);
  void SetPostOfficeBox(const string16& post_office_box);
  void SetNeighborhood(const string16& neighborhood);
  void SetRegion(const string16& region);
  void SetCity(const string16& city);
  void SetPostalCode(const string16& postal_code);
  void SetCountry(const string16& country);

  void SetEmailAddresses(const std::vector<string16>& email_addresses);
  void SetPhoneNumbers(const std::vector<string16>& phone_numbers);

 private:
  string16 street_;
  string16 post_office_box_;
  string16 neighborhood_;
  string16 region_;
  string16 city_;
  string16 postal_code_;
  string16 country_;
  string16 first_name_;
  string16 middle_name_;
  string16 last_name_;
  string16 suffix_;
  std::vector<string16> email_addresses_;
  std::vector<string16> phone_numbers_;
};

#endif  // COMPONENTS_AUTOFILL_BROWSER_ANDROID_TEST_AUXILIARY_PROFILE_LOADER_ANDROID_H_
