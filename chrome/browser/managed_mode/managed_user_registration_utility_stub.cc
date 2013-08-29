// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_utility_stub.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

ManagedUserRegistrationUtilityStub::ManagedUserRegistrationUtilityStub()
    : register_was_called_(false) {
}

ManagedUserRegistrationUtilityStub::~ManagedUserRegistrationUtilityStub() {
}

void ManagedUserRegistrationUtilityStub::Register(
    const std::string& managed_user_id,
    const ManagedUserRegistrationInfo& info,
    const RegistrationCallback& callback) {
  DCHECK(!register_was_called_);
  register_was_called_ = true;
  callback_ = callback;
  managed_user_id_ = managed_user_id;
  display_name_ = info.name;
  master_key_ = info.master_key;
}

void ManagedUserRegistrationUtilityStub::RunSuccessCallback(
    const std::string& token) {
  if (callback_.is_null())
    return;
  callback_.Run(GoogleServiceAuthError(GoogleServiceAuthError::NONE), token);
  callback_.Reset();
}

void ManagedUserRegistrationUtilityStub::RunFailureCallback(
    const GoogleServiceAuthError::State state) {
  if (callback_.is_null())
    return;
  GoogleServiceAuthError error(state);
  callback_.Run(error, std::string());
  callback_.Reset();
}
