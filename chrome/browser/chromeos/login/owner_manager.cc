// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace chromeos {

OwnerManager::OwnerManager()
    : private_key_(NULL),
      public_key_(NULL),
      utils_(OwnerKeyUtils::Create()) {
}

OwnerManager::~OwnerManager() {}

void OwnerManager::LoadOwnerKey() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  LOG(INFO) << "Loading owner key";
  NotificationType result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;

  // If |public_key_| isn't empty, we already have the key, so don't
  // try to import again.
  if (public_key_.empty() &&
      !utils_->ImportPublicKey(utils_->GetOwnerKeyFilePath(), &public_key_)) {
    result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED;
  }

  // Whether we loaded the public key or not, send a notification indicating
  // that we're done with this attempt.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::SendNotification,
                        result,
                        NotificationService::NoDetails()));
}

void OwnerManager::GenerateKeysAndExportPublic() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  LOG(INFO) << "Generating key pair";

  private_key_.reset(utils_->GenerateKeyPair());

  if (private_key_.get() && private_key_->ExportPublicKey(&public_key_)) {
    // If we generated the keys successfully, export them.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &OwnerManager::ExportKey));
  } else {
    private_key_.reset(NULL);
    // If we didn't generate the key, send along a notification of failure.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::SendNotification,
                          NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED,
                          NotificationService::NoDetails()));
  }
}

void OwnerManager::ExportKey() {
  LOG(INFO) << "Exporting public key";
  if (!utils_->ExportPublicKeyViaDbus(private_key_.get(), this)) {
    private_key_.reset(NULL);
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::SendNotification,
                          NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED,
                          NotificationService::NoDetails()));
  }
}

void OwnerManager::Run(bool value) {
  LOG(INFO) << "Export public key attempt: " << (value ? "success" : "fail");
  NotificationType result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;
  if (!value)
    result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED;

  // Whether we exported the public key or not, send a notification indicating
  // that we're done with this attempt.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
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

void OwnerManager::Sign(const ChromeThread::ID thread_id,
                        const std::string& data,
                        Delegate* d) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // If it's not the case that we can get both keys...
  if (!(EnsurePublicKey() && EnsurePrivateKey())) {
    ChromeThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::CallDelegate,
                          d, KEY_UNAVAILABLE, std::vector<uint8>()));
    return;
  }

  LOG(INFO) << "Starting signing attempt";
  KeyOpCode return_code = SUCCESS;
  std::vector<uint8> signature;
  if (!utils_->Sign(data, &signature, private_key_.get())) {
    return_code = OPERATION_FAILED;
  }

  ChromeThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, return_code, signature));
}

void OwnerManager::Verify(const ChromeThread::ID thread_id,
                          const std::string& data,
                          const std::vector<uint8>& signature,
                          Delegate* d) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (!EnsurePublicKey()) {
    ChromeThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::CallDelegate,
                          d, KEY_UNAVAILABLE, std::vector<uint8>()));
    return;
  }

  LOG(INFO) << "Starting verify attempt";
  KeyOpCode return_code = SUCCESS;
  if (!utils_->Verify(data, signature, public_key_)) {
    return_code = OPERATION_FAILED;
  }
  ChromeThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, return_code, std::vector<uint8>()));
}

void OwnerManager::SendNotification(NotificationType type,
                                    const NotificationDetails& details) {
    NotificationService::current()->Notify(
        type,
        NotificationService::AllSources(),
        details);
}

}  // namespace chromeos
