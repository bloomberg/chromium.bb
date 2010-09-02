// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace chromeos {

//  static
OwnershipService* OwnershipService::GetSharedInstance() {
  return Singleton<OwnershipService>::get();
}

OwnershipService::OwnershipService()
    : manager_(new OwnerManager),
      utils_(OwnerKeyUtils::Create()) {
}

OwnershipService::~OwnershipService() {}


bool OwnershipService::IsAlreadyOwned() {
  return file_util::PathExists(utils_->GetOwnerKeyFilePath());
}

bool OwnershipService::StartLoadOwnerKeyAttempt() {
  if (!IsAlreadyOwned()) {
    LOG(WARNING) << "Device not yet owned";
    return false;
  }
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(manager_.get(), &OwnerManager::LoadOwnerKey));
  return true;
}

bool OwnershipService::StartTakeOwnershipAttempt() {
  if (IsAlreadyOwned()) {
    LOG(ERROR) << "Device is already owned";
    return false;
  }
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(manager_.get(),
                        &OwnerManager::GenerateKeysAndExportPublic));
  return true;
}

void OwnershipService::StartSigningAttempt(const std::string& data,
                                           OwnerManager::Delegate* d) {
  if (!IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    d->OnKeyOpComplete(OwnerManager::KEY_UNAVAILABLE, std::vector<uint8>());
    return;
  }
  ChromeThread::ID thread_id;
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = ChromeThread::UI;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(manager_.get(),
                        &OwnerManager::Sign,
                        thread_id,
                        data, d));
  return;
}

void OwnershipService::StartVerifyAttempt(const std::string& data,
                                          const std::vector<uint8>& signature,
                                          OwnerManager::Delegate* d) {
  if (!IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    d->OnKeyOpComplete(OwnerManager::KEY_UNAVAILABLE, std::vector<uint8>());
    return;
  }
  ChromeThread::ID thread_id;
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = ChromeThread::UI;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(manager_.get(),
                        &OwnerManager::Verify,
                        thread_id,
                        data,
                        signature,
                        d));
  return;
}

bool OwnershipService::CurrentUserIsOwner() {
  // If this user has the private key associated with the owner's
  // public key, this user is the owner.
  return IsAlreadyOwned() && manager_->EnsurePrivateKey();
}

}  // namespace chromeos
