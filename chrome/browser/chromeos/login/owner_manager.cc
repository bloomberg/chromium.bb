// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace chromeos {

OwnerManager::OwnerManager()
    : private_key_(NULL),
      public_key_(NULL),
      utils_(OwnerKeyUtils::Create()) {
}

OwnerManager::~OwnerManager() {}

void OwnerManager::LoadOwnerKey() {
  BootTimesLoader::Get()->AddLoginTimeMarker("LoadOwnerKeyStart", false);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  VLOG(1) << "Loading owner key";
  NotificationType result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;

  // If |public_key_| isn't empty, we already have the key, so don't
  // try to import again.
  if (public_key_.empty() &&
      !utils_->ImportPublicKey(utils_->GetOwnerKeyFilePath(), &public_key_)) {
    result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED;
  }

  // Whether we loaded the public key or not, send a notification indicating
  // that we're done with this attempt.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::SendNotification,
                        result,
                        NotificationService::NoDetails()));
}

bool OwnerManager::EnsurePublicKey() {
  if (public_key_.empty())
    LoadOwnerKey();

  return !public_key_.empty();
}

bool OwnerManager::EnsurePrivateKey() {
  if (!EnsurePublicKey())
    return false;

  if (!private_key_.get())
    private_key_.reset(utils_->FindPrivateKey(public_key_));

  return private_key_.get() != NULL;
}

void OwnerManager::Sign(const BrowserThread::ID thread_id,
                        const std::string& data,
                        Delegate* d) {
  BootTimesLoader::Get()->AddLoginTimeMarker("SignStart", false);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // If it's not the case that we can get both keys...
  if (!(EnsurePublicKey() && EnsurePrivateKey())) {
    BrowserThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::CallDelegate,
                          d, KEY_UNAVAILABLE, std::vector<uint8>()));
    BootTimesLoader::Get()->AddLoginTimeMarker("SignEnd", false);
    return;
  }

  VLOG(1) << "Starting signing attempt";
  KeyOpCode return_code = SUCCESS;
  std::vector<uint8> signature;
  if (!utils_->Sign(data, &signature, private_key_.get())) {
    return_code = OPERATION_FAILED;
  }

  BrowserThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, return_code, signature));
  BootTimesLoader::Get()->AddLoginTimeMarker("SignEnd", false);
}

void OwnerManager::Verify(const BrowserThread::ID thread_id,
                          const std::string& data,
                          const std::vector<uint8>& signature,
                          Delegate* d) {
  BootTimesLoader::Get()->AddLoginTimeMarker("VerifyStart", false);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!EnsurePublicKey()) {
    BrowserThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::CallDelegate,
                          d, KEY_UNAVAILABLE, std::vector<uint8>()));
    BootTimesLoader::Get()->AddLoginTimeMarker("VerifyEnd", false);
    return;
  }

  VLOG(1) << "Starting verify attempt";
  KeyOpCode return_code = SUCCESS;
  if (!utils_->Verify(data, signature, public_key_)) {
    return_code = OPERATION_FAILED;
  }
  BrowserThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, return_code, std::vector<uint8>()));
  BootTimesLoader::Get()->AddLoginTimeMarker("VerifyEnd", false);
}

void OwnerManager::SendNotification(NotificationType type,
                                    const NotificationDetails& details) {
    NotificationService::current()->Notify(
        type,
        NotificationService::AllSources(),
        details);
}

}  // namespace chromeos
