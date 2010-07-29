// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"

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

bool OwnerManager::IsAlreadyOwned() {
  return file_util::PathExists(utils_->GetOwnerKeyFilePath());
}

bool OwnerManager::StartLoadOwnerKeyAttempt() {
  if (!IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    return false;
  }
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &OwnerManager::LoadOwnerKey));
  return true;
}

bool OwnerManager::StartTakeOwnershipAttempt() {
  if (IsAlreadyOwned()) {
    LOG(ERROR) << "Device is already owned";
    return false;
  }
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &OwnerManager::GenerateKeysAndExportPublic));
  return true;
}

bool OwnerManager::StartSigningAttempt(const std::string& data, Delegate* d) {
  if (!IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    return false;
  }
  ChromeThread::ID thread_id;
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = ChromeThread::UI;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &OwnerManager::Sign, thread_id, data, d));
  return true;
}

bool OwnerManager::StartVerifyAttempt(const std::string& data,
                                      const std::string& signature,
                                      Delegate* d) {
  if (!IsAlreadyOwned()) {
    LOG(ERROR) << "Device not yet owned";
    return false;
  }
  ChromeThread::ID thread_id;
  if (!ChromeThread::GetCurrentThreadIdentifier(&thread_id))
    thread_id = ChromeThread::UI;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::Verify,
                        thread_id,
                        data,
                        signature,
                        d));
  return true;
}

void OwnerManager::LoadOwnerKey() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  public_key_ = utils_->ImportPublicKey(utils_->GetOwnerKeyFilePath());

  // Whether we loaded the public key or not, send a notification indicating
  // that we're done with this attempt.  We send along the key if we
  // got it, NULL if not.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::SendNotification,
                        NotificationType::OWNER_KEY_FETCH_ATTEMPT_COMPLETE,
                        Details<SECKEYPublicKey*>(&public_key_)));
}

void OwnerManager::GenerateKeysAndExportPublic() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  public_key_ = NULL;
  private_key_ = NULL;

  if (utils_->GenerateKeyPair(&private_key_, &public_key_)) {
    // If we generated the keys successfully, export them.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &OwnerManager::ExportKey));
  } else {
    // If we didn't generate the key, send along NULL with the notification
    // that we're done with this attempt.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::SendNotification,
                          NotificationType::OWNER_KEY_FETCH_ATTEMPT_COMPLETE,
                          Details<SECKEYPublicKey*>(&public_key_)));
  }
}

void OwnerManager::ExportKey() {
  if (!utils_->ExportPublicKeyViaDbus(public_key_)) {
    utils_->DestroyKeys(private_key_, public_key_);
    private_key_ = NULL;
    public_key_ = NULL;
  }

  // Whether we generated the keys or not, send a notification indicating
  // that we're done with this attempt.  We send along the public key if we
  // got it, NULL if not.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::SendNotification,
                        NotificationType::OWNER_KEY_FETCH_ATTEMPT_COMPLETE,
                        Details<SECKEYPublicKey*>(&public_key_)));
}

bool OwnerManager::EnsurePublicKey() {
  if (!public_key_)
    LoadOwnerKey();

  return public_key_ != NULL;
}

bool OwnerManager::EnsurePrivateKey() {
  if (!EnsurePublicKey())
    return false;

  if (!private_key_)
    private_key_ = utils_->FindPrivateKey(public_key_);

  return private_key_ != NULL;
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
                          d, KEY_UNAVAILABLE, std::string()));
    return;
  }

  // TODO(cmasone): Sign |data| with |private_key_|, return
  // appropriate errors via CallDelegate.
  ChromeThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, SUCCESS, data));
}

void OwnerManager::Verify(const ChromeThread::ID thread_id,
                          const std::string& data,
                          const std::string& signature,
                          Delegate* d) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (!EnsurePublicKey()) {
    ChromeThread::PostTask(
        thread_id, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnerManager::CallDelegate,
                          d, KEY_UNAVAILABLE, std::string()));
    return;
  }

  LOG(INFO) << "Starting verify attempt";
  // TODO(cmasone): Verify |signature| over |data| with |public_key_|, return
  // appropriate errors via CallDelegate.
  ChromeThread::PostTask(
      thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &OwnerManager::CallDelegate,
                        d, SUCCESS, std::string()));
}

void OwnerManager::SendNotification(NotificationType type,
                                    const NotificationDetails& details) {
    NotificationService::current()->Notify(
        type,
        NotificationService::AllSources(),
        details);
}

}  // namespace chromeos
