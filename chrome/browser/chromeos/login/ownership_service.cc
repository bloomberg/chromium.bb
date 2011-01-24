// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_thread.h"

namespace chromeos {

static base::LazyInstance<OwnershipService> g_ownership_service(
    base::LINKER_INITIALIZED);

//  static
OwnershipService* OwnershipService::GetSharedInstance() {
  return g_ownership_service.Pointer();
}

OwnershipService::OwnershipService()
    : manager_(new OwnerManager),
      utils_(OwnerKeyUtils::Create()) {
}

OwnershipService::~OwnershipService() {}


bool OwnershipService::IsAlreadyOwned() {
  // This should not do blocking IO from the UI thread.
  // Temporarily allow it for now. http://crbug.com/70097
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return file_util::PathExists(utils_->GetOwnerKeyFilePath());
}

bool OwnershipService::StartLoadOwnerKeyAttempt() {
  if (!IsAlreadyOwned()) {
    LOG(WARNING) << "Device not yet owned";
    return false;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager_.get(), &OwnerManager::LoadOwnerKey));
  return true;
}

bool OwnershipService::StartTakeOwnershipAttempt(const std::string& owner) {
  if (IsAlreadyOwned()) {
    VLOG(1) << "Device is already owned";
    return false;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
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
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
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
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
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
