// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/android/test_auxiliary_profile_loader_android.h"

TestAuxiliaryProfileLoader::TestAuxiliaryProfileLoader() {
}

TestAuxiliaryProfileLoader::~TestAuxiliaryProfileLoader() {
}

bool TestAuxiliaryProfileLoader::GetHasPermissions() const {
  return true;
}

string16 TestAuxiliaryProfileLoader::GetFirstName() const {
  return first_name_;
}

string16 TestAuxiliaryProfileLoader::GetMiddleName() const {
  return middle_name_;
}

string16 TestAuxiliaryProfileLoader::GetLastName() const {
  return last_name_;
}

string16 TestAuxiliaryProfileLoader::GetSuffix() const {
  return suffix_;
}

string16 TestAuxiliaryProfileLoader::GetStreet() const {
  return street_;
}

string16 TestAuxiliaryProfileLoader::GetPostOfficeBox() const {
  return post_office_box_;
}

string16 TestAuxiliaryProfileLoader::GetCity() const {
  return city_;
}

string16 TestAuxiliaryProfileLoader::GetNeighborhood() const {
  return neighborhood_;
}

string16 TestAuxiliaryProfileLoader::GetRegion() const {
  return region_;
}

string16 TestAuxiliaryProfileLoader::GetPostalCode() const {
  return postal_code_;
}

string16 TestAuxiliaryProfileLoader::GetCountry() const {
  return country_;
}

void TestAuxiliaryProfileLoader::GetEmailAddresses(
    std::vector<string16>* email_addresses) const {
  *email_addresses = email_addresses_;
}

void TestAuxiliaryProfileLoader::GetPhoneNumbers(
    std::vector<string16>* phone_numbers) const {
  *phone_numbers = phone_numbers_;
}

void TestAuxiliaryProfileLoader::SetFirstName(const string16& first_name) {
  first_name_ = first_name;
}

void TestAuxiliaryProfileLoader::SetMiddleName(const string16& middle_name) {
  middle_name_ = middle_name;
}

void TestAuxiliaryProfileLoader::SetLastName(const string16& last_name) {
  last_name_ = last_name;
}

void TestAuxiliaryProfileLoader::SetSuffix(const string16& suffix) {
  suffix_ = suffix;
}

void TestAuxiliaryProfileLoader::SetStreet(const string16& street) {
  street_ = street;
}

void TestAuxiliaryProfileLoader::SetPostOfficeBox(
    const string16& post_office_box) {
  post_office_box_ = post_office_box;
}

void TestAuxiliaryProfileLoader::SetNeighborhood(
    const string16& neighborhood) {
  neighborhood_ = neighborhood;
}

void TestAuxiliaryProfileLoader::SetRegion(const string16& region) {
  region_ = region;
}

void TestAuxiliaryProfileLoader::SetCity(const string16& city) {
  city_ = city;
}

void TestAuxiliaryProfileLoader::SetPostalCode(const string16& postal_code) {
  postal_code_ = postal_code;
}

void TestAuxiliaryProfileLoader::SetCountry(const string16& country) {
  country_ = country;
}

void TestAuxiliaryProfileLoader::SetEmailAddresses(
    const std::vector<string16>& addresses) {
  email_addresses_ = addresses;
}

void TestAuxiliaryProfileLoader::SetPhoneNumbers(
    const std::vector<string16>& phone_numbers) {
  phone_numbers_ = phone_numbers;
}
