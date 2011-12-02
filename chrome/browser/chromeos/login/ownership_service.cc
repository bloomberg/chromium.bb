// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace chromeos {

static base::LazyInstance<OwnershipService> g_ownership_service =
    LAZY_INSTANCE_INITIALIZER;

//  static
OwnershipService* OwnershipService::GetSharedInstance() {
  return g_ownership_service.Pointer();
}

OwnershipService::OwnershipService()
    : manager_(new OwnerManager),
      utils_(OwnerKeyUtils::Create()),
      ownership_status_(OWNERSHIP_UNKNOWN) {
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
      content::NotificationService::AllSources());
}

OwnershipService::~OwnershipService() {}

void OwnershipService::Prewarm() {
  // Note that we cannot prewarm in constructor because in current codebase
  // object is created before spawning threads.
  if (g_ownership_service == this) {
    // Start getting ownership status.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&OwnershipService::FetchStatus, base::Unretained(this)));
  } else {
    // This can happen only for particular test: OwnershipServiceTest. It uses
    // mocks and for that uses OwnershipService not as a regular singleton but
    // as a resurrecting object. This behaviour conflicts with
    // DISABLE_RUNNABLE_METHOD_REFCOUNT.  So avoid posting task in those
    // circumstances in order to avoid accessing already deleted object.
  }
}

bool OwnershipService::IsAlreadyOwned() {
  return file_util::PathExists(utils_->GetOwnerKeyFilePath());
}

OwnershipService::Status OwnershipService::GetStatus(bool blocking) {
  Status status = OWNERSHIP_UNKNOWN;
  bool is_owned = false;
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    ownership_status_lock_.Acquire();
    status = ownership_status_;
    ownership_status_lock_.Release();
    if (status != OWNERSHIP_UNKNOWN || !blocking)
      return status;
    // Under common usage there is very short lapse of time when ownership
    // status is still unknown after constructing OwnershipService.
    LOG(ERROR) << "Blocking on UI thread in OwnershipService::GetStatus";
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    is_owned = IsAlreadyOwned();
  } else {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    is_owned = IsAlreadyOwned();
  }
  status = is_owned ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  SetStatus(status);
  return status;
}

void OwnershipService::StartLoadOwnerKeyAttempt() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TryLoadOwnerKeyAttempt, base::Unretained(this)));
}

void OwnershipService::StartUpdateOwnerKey(const std::vector<uint8>& new_key,
                                           OwnerManager::KeyUpdateDelegate* d) {
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnershipService::UpdateOwnerKey, base::Unretained(this),
                 thread_id, new_key, d));
  return;
}

void OwnershipService::StartSigningAttempt(const std::string& data,
                                           OwnerManager::Delegate* d) {
  BrowserThread::ID thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = BrowserThread::UI;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnershipService::TrySigningAttempt, base::Unretained(this),
                 thread_id, data, d));
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
      base::Bind(&OwnershipService::TryVerifyAttempt, base::Unretained(this),
                 thread_id, data, signature, d));
  return;
}

void OwnershipService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    SetStatus(OWNERSHIP_TAKEN);
    notification_registrar_.RemoveAll();
  } else {
    NOTREACHED();
  }
}

bool OwnershipService::CurrentUserIsOwner() {
  // If this user has the private key associated with the owner's
  // public key, this user is the owner.
  return IsAlreadyOwned() && manager_->EnsurePrivateKey();
}

// static
void OwnershipService::UpdateOwnerKey(OwnershipService* service,
                                      const BrowserThread::ID thread_id,
                                      const std::vector<uint8>& new_key,
                                      OwnerManager::KeyUpdateDelegate* d) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  service->manager()->UpdateOwnerKey(thread_id, new_key, d);
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
void OwnershipService::TrySigningAttempt(OwnershipService* service,
                                         const BrowserThread::ID thread_id,
                                         const std::string& data,
                                         OwnerManager::Delegate* d) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!service->IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    BrowserThread::PostTask(
        thread_id, FROM_HERE,
        base::Bind(&OwnershipService::FailAttempt, d));
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
        base::Bind(&OwnershipService::FailAttempt, d));
    return;
  }
  service->manager()->Verify(thread_id, data, signature, d);
}

// static
void OwnershipService::FailAttempt(OwnerManager::Delegate* d) {
  d->OnKeyOpComplete(OwnerManager::KEY_UNAVAILABLE, std::vector<uint8>());
}

void OwnershipService::FetchStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Status status = IsAlreadyOwned() ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  SetStatus(status);
}

void OwnershipService::SetStatus(Status new_status) {
  DCHECK(new_status == OWNERSHIP_TAKEN || new_status == OWNERSHIP_NONE);
  base::AutoLock lk(ownership_status_lock_);
  ownership_status_ = new_status;
}

}  // namespace chromeos
