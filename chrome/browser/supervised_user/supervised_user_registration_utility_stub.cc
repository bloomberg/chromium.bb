// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_registration_utility_stub.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

SupervisedUserRegistrationUtilityStub::SupervisedUserRegistrationUtilityStub()
    : register_was_called_(false) {
}

SupervisedUserRegistrationUtilityStub::
~SupervisedUserRegistrationUtilityStub() {
}

void SupervisedUserRegistrationUtilityStub::Register(
    const std::string& supervised_user_id,
    const SupervisedUserRegistrationInfo& info,
    const RegistrationCallback& callback) {
  DCHECK(!register_was_called_);
  register_was_called_ = true;
  callback_ = callback;
  supervised_user_id_ = supervised_user_id;
  display_name_ = info.name;
  master_key_ = info.master_key;
}

void SupervisedUserRegistrationUtilityStub::RunSuccessCallback(
    const std::string& token) {
  if (callback_.is_null())
    return;
  callback_.Run(GoogleServiceAuthError(GoogleServiceAuthError::NONE), token);
  callback_.Reset();
}

void SupervisedUserRegistrationUtilityStub::RunFailureCallback(
    const GoogleServiceAuthError::State state) {
  if (callback_.is_null())
    return;
  GoogleServiceAuthError error(state);
  callback_.Run(error, std::string());
  callback_.Reset();
}
