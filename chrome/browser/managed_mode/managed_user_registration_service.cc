// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_service.h"

#include "base/bind.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "google_apis/gaia/google_service_auth_error.h"

ManagedUserRegistrationService::ManagedUserRegistrationService()
    : weak_ptr_factory_(this) {
}

ManagedUserRegistrationService::~ManagedUserRegistrationService() {
  DCHECK(callback_.is_null());
}

void ManagedUserRegistrationService::Register(
    const string16& name,
    const RegistrationCallback& callback) {
  callback_ = callback;
  // TODO(bauerb): This is a stub implementation.
  DispatchCallback();
}

ProfileManager::CreateCallback
ManagedUserRegistrationService::GetRegistrationAndInitCallback() {
  return base::Bind(&ManagedUserRegistrationService::OnProfileCreated,
                    weak_ptr_factory_.GetWeakPtr());
}

void ManagedUserRegistrationService::DispatchCallback() {
  // TODO(bauerb): This is a stub implementation.
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  std::string token("abcdef");
  callback_.Run(error, token);
  callback_.Reset();
}

void ManagedUserRegistrationService::OnProfileCreated(
    Profile* profile,
    Profile::CreateStatus status) {
  // We're being called back twice: once after the profile has been created on
  // disk, and once after all the profile services (including the
  // ManagedUserService) have been initialized. Ignore the first one.
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  DCHECK(managed_user_service->ProfileIsManaged());
  managed_user_service->RegisterAndInitSync(this);
}
