// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/fake_supervised_user_manager.h"

#include <string>

namespace {

// As defined in /chromeos/dbus/cryptohome_client.cc.
static const char kUserIdHashSuffix[] = "-hash";

}  // namespace

namespace chromeos {

FakeSupervisedUserManager::FakeSupervisedUserManager()  {}

FakeSupervisedUserManager::~FakeSupervisedUserManager() {
}

const User* FakeSupervisedUserManager::CreateUserRecord(
    const std::string& manager_id,
    const std::string& local_user_id,
    const std::string& sync_user_id,
    const string16& display_name) {
  return NULL;
}

std::string FakeSupervisedUserManager::GenerateUserId() {
  return std::string();
}

const User* FakeSupervisedUserManager::FindByDisplayName(
    const string16& display_name) const {
  return NULL;
}

const User* FakeSupervisedUserManager::FindBySyncId(
    const std::string& sync_id) const {
  return NULL;
}

std::string FakeSupervisedUserManager::GetUserSyncId(
    const std::string& managed_user_id) const {
  return std::string();
}

string16 FakeSupervisedUserManager::GetManagerDisplayName(
    const std::string& managed_user_id) const {
  return string16();
}

std::string FakeSupervisedUserManager::GetManagerUserId(
    const std::string& managed_user_id) const {
  return std::string();
}

std::string FakeSupervisedUserManager::GetManagerDisplayEmail(
    const std::string& managed_user_id) const {
  return std::string();
}

}  // namespace chromeos
