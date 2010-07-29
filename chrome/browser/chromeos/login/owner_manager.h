// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/owner_key_utils.h"
#include "chrome/browser/chrome_thread.h"

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
                                 const std::string& payload) = 0;
  };

  OwnerManager();
  virtual ~OwnerManager();

  bool IsAlreadyOwned();

  // If the device has been owned already, posts a task to the FILE thread to
  // fetch the public key off disk.
  // Returns true if the attempt was initiated, false otherwise.
  //
  // Sends out a OWNER_KEY_FETCH_ATTEMPT_COMPLETE notification on completion.
  // Notification comes with a Details<SECKEYPublicKey*> that contains a pointer
  // to the public key, or NULL if the fetch attempt failed.
  bool StartLoadOwnerKeyAttempt();

  // If the device has not yet been owned, posts a task to the FILE
  // thread to generate the owner's keys and put them in the right
  // places.  Keeps them in memory as well, for later use.
  // Returns true if the attempt was initiated, false otherwise.
  //
  // Sends out a OWNER_KEY_FETCH_ATTEMPT_COMPLETE notification on completion.
  // Notification comes with a Details<SECKEYPublicKey*> that contains a pointer
  // to the public key, or NULL if the fetch attempt failed.
  bool StartTakeOwnershipAttempt();

  // Initiate an attempt to sign |data| with |private_key_|.  Will call
  // d->OnKeyOpComplete() when done.  Upon success, the signature will be passed
  // as the |payload| argument to d->OnKeyOpComplete().
  // Returns true if the attempt was initiated, false otherwise.
  //
  // If you call this on a well-known thread, you'll be called back on that
  // thread.  Otherwise, you'll get called back on the UI thread.
  bool StartSigningAttempt(const std::string& data, Delegate* d);

  // Initiate an attempt to verify that |signature| is valid over |data| with
  // |public_key_|.  When the attempt is completed, an appropriate KeyOpCode
  // will be passed to d->OnKeyOpComplete().
  // Returns true if the attempt was initiated, false otherwise.
  //
  // If you call this on a well-known thread, you'll be called back on that
  // thread.  Otherwise, you'll get called back on the UI thread.
  bool StartVerifyAttempt(const std::string& data,
                          const std::string& signature,
                          Delegate* d);

 private:
  // Pulls the owner's public key off disk and into memory.
  //
  // Call this on the FILE thread.
  void LoadOwnerKey();

  // Generates the owner's keys in the default NSS token.  Also stores
  // them in |public_key_| and |private_key_|.  When done, causes the
  // public key to get exported via DBus.
  //
  // Call this on the FILE thread.
  void GenerateKeysAndExportPublic();

  // Exports |public_key_| via DBus.
  //
  // Call this on the UI thread (because of DBus usage).
  void ExportKey();

  bool EnsurePublicKey();
  bool EnsurePrivateKey();

  // Do the actual work of signing |data| with |private_key_|.  First,
  // ensures that we have the keys we need.  Then, computes the signature.
  //
  // On success, calls d->OnKeyOpComplete() on |thread_id| with a
  // successful return code, passing the signaure blob in |payload|.
  // On failure, calls d->OnKeyOpComplete() on |thread_id| with an appropriate
  // error and passes an empty string for |payload|.
  void Sign(const ChromeThread::ID thread_id,
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
  void Verify(const ChromeThread::ID thread_id,
              const std::string& data,
              const std::string& signature,
              Delegate* d);

  // A helper method to send a notification on another thread.
  void SendNotification(NotificationType type,
                        const NotificationDetails& details);

  // A helper method to call back a delegte on another thread.
  void CallDelegate(Delegate* d,
                    const KeyOpCode return_code,
                    const std::string& payload) {
    d->OnKeyOpComplete(return_code, payload);
  }

  SECKEYPrivateKey* private_key_;
  SECKEYPublicKey* public_key_;

  scoped_ptr<OwnerKeyUtils> utils_;

  friend class OwnerManagerTest;

  DISALLOW_COPY_AND_ASSIGN(OwnerManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
