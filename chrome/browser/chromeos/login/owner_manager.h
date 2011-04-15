// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "crypto/rsa_private_key.h"
#include "chrome/browser/chromeos/login/owner_key_utils.h"
#include "content/browser/browser_thread.h"

class FilePath;
class NotificationDetails;
class NotificationType;

namespace chromeos {

// This class allows the registration of an Owner of a Chromium OS device.
// It handles generating the appropriate keys and storing them in the
// appropriate locations.
class OwnerManager : public base::RefCountedThreadSafe<OwnerManager> {
 public:
  // Return codes for public/private key operations.
  enum KeyOpCode {
    SUCCESS,
    KEY_UNAVAILABLE,  // The necessary key isn't available yet.
    OPERATION_FAILED  // The crypto operation failed.
  };

  class Delegate {
   public:
    // Upon completion of a key operation, this method will be called.
    // |return_code| indicates what happened, |payload| will be used to pass
    // back any artifacts of the operation.  For example, if the operation
    // was a signature attempt, the signature blob would come back in |payload|.
    virtual void OnKeyOpComplete(const KeyOpCode return_code,
                                 const std::vector<uint8>& payload) = 0;
  };

  class KeyUpdateDelegate {
   public:
    // Called upon completion of a key update operation.
    virtual void OnKeyUpdated() = 0;
  };

  OwnerManager();
  virtual ~OwnerManager();

  // Sets a new owner key from a provided memory buffer.
  void UpdateOwnerKey(const BrowserThread::ID thread_id,
                      const std::vector<uint8>& key,
                      KeyUpdateDelegate* d);

  // Pulls the owner's public key off disk and into memory.
  //
  // Call this on the FILE thread.
  void LoadOwnerKey();

  bool EnsurePublicKey();
  bool EnsurePrivateKey();

  // Do the actual work of signing |data| with |private_key_|.  First,
  // ensures that we have the keys we need.  Then, computes the signature.
  //
  // On success, calls d->OnKeyOpComplete() on |thread_id| with a
  // successful return code, passing the signaure blob in |payload|.
  // On failure, calls d->OnKeyOpComplete() on |thread_id| with an appropriate
  // error and passes an empty string for |payload|.
  void Sign(const BrowserThread::ID thread_id,
            const std::string& data,
            Delegate* d);

  // Do the actual work of verifying that |signature| is valid over
  // |data| with |public_key_|.  First, ensures we have the key we
  // need, then does the verify.
  //
  // On success, calls d->OnKeyOpComplete() on |thread_id| with a
  // successful return code, passing an empty string for |payload|.
  // On failure, calls d->OnKeyOpComplete() on |thread_id| with an appropriate
  // error code, passing an empty string for |payload|.
  void Verify(const BrowserThread::ID thread_id,
              const std::string& data,
              const std::vector<uint8>& signature,
              Delegate* d);

 private:
  // A helper method to send a notification on another thread.
  void SendNotification(NotificationType type,
                        const NotificationDetails& details);

  // Calls back a key update delegate on a given thread.
  void CallKeyUpdateDelegate(KeyUpdateDelegate* d) {
    d->OnKeyUpdated();
  }

  // A helper method to call back a delegte on another thread.
  void CallDelegate(Delegate* d,
                    const KeyOpCode return_code,
                    const std::vector<uint8>& payload) {
    d->OnKeyOpComplete(return_code, payload);
  }

  scoped_ptr<crypto::RSAPrivateKey> private_key_;
  std::vector<uint8> public_key_;

  scoped_refptr<OwnerKeyUtils> utils_;

  friend class OwnerManagerTest;

  DISALLOW_COPY_AND_ASSIGN(OwnerManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
