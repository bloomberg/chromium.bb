// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
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
  return file_util::PathExists(utils_->GetOwnerKeyFilePath());
}

void OwnershipService::StartLoadOwnerKeyAttempt() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&TryLoadOwnerKeyAttempt, this));
}

void OwnershipService::StartTakeOwnershipAttempt(const std::string& unused) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&OwnershipService::TryTakeOwnershipAttempt, this));
}

void OwnershipService::StartSigningAttempt(const std::string& data,
                                           OwnerManager::Delegate* d) {
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&OwnershipService::TrySigningAttempt,
                          this,
                          thread_id,
                          data,
                          d));
  return;
}

void OwnershipService::StartVerifyAttempt(const std::string& data,
                                          const std::vector<uint8>& signature,
                                          OwnerManager::Delegate* d) {
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&OwnershipService::TryVerifyAttempt,
                          this,
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

// static
void OwnershipService::TryLoadOwnerKeyAttempt(OwnershipService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!service->IsAlreadyOwned()) {
    VLOG(1) << "Device not yet owned";
    return;
  }
  service->manager()->LoadOwnerKey();
}

// static
void OwnershipService::TryTakeOwnershipAttempt(OwnershipService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (service->IsAlreadyOwned()) {
    VLOG(1) << "Device is already owned";
    return;
  }
  service->manager()->GenerateKeysAndExportPublic();
}

// static
void OwnershipService::TrySigningAttempt(OwnershipService* service,
                                         const BrowserThread::ID thread_id,
                                         const std::string& data,
                                         OwnerManager::Delegate* d) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!service->IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    BrowserThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableFunction(&OwnershipService::FailAttempt, d));
    return;
  }
  service->manager()->Sign(thread_id, data, d);
}

// static
void OwnershipService::TryVerifyAttempt(OwnershipService* service,
                                        const BrowserThread::ID thread_id,
                                        const std::string& data,
                                        const std::vector<uint8>& signature,
                                        OwnerManager::Delegate* d) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!service->IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    BrowserThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableFunction(&OwnershipService::FailAttempt, d));
    return;
  }
  service->manager()->Verify(thread_id, data, signature, d);
}

// static
void OwnershipService::FailAttempt(OwnerManager::Delegate* d) {
  d->OnKeyOpComplete(OwnerManager::KEY_UNAVAILABLE, std::vector<uint8>());
}

}  // namespace chromeos
