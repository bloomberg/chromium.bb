// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_
#define CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace base {
class TaskRunner;
}

namespace chromeos {
class CryptohomeClient;
}

namespace chromeos {

// Information retrieved from cryptohome by TPMTokenInfoGetter.
// For invalid token |token_name| and |user_pin| will be empty, while
// |token_slot_id| will be set to -1.
struct TPMTokenInfo {
  // Default constructor creates token info for disabled TPM.
  TPMTokenInfo();
  ~TPMTokenInfo();

  bool tpm_is_enabled;
  std::string token_name;
  std::string user_pin;
  int token_slot_id;
};

// Class for getting a user or the system TPM token info from cryptohome during
// TPM token loading.
class CHROMEOS_EXPORT TPMTokenInfoGetter {
 public:
  using TPMTokenInfoCallback = base::Callback<void(const TPMTokenInfo& info)>;

  // Factory method for TPMTokenInfoGetter for a user token.
  static scoped_ptr<TPMTokenInfoGetter> CreateForUserToken(
      const std::string& user_id,
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  // Factory method for TPMTokenGetter for the system token.
  static scoped_ptr<TPMTokenInfoGetter> CreateForSystemToken(
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  ~TPMTokenInfoGetter();

  // Starts getting TPM token info. Should be called at most once.
  // |callback| will be called when all the info is fetched.
  // The object may get deleted before |callback| is called, which is equivalent
  // to cancelling the info getting (in which case |callback| will never get
  // called).
  void Start(const TPMTokenInfoCallback& callback);

 private:
  enum Type {
    TYPE_SYSTEM,
    TYPE_USER
  };

  enum State {
    STATE_INITIAL,
    STATE_STARTED,
    STATE_TPM_ENABLED,
    STATE_DONE
  };

  TPMTokenInfoGetter(
      Type type,
      const std::string& user_id,
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  // Continues TPM token info getting procedure by starting the task associated
  // with the current TPMTokenInfoGetter state.
  void Continue();

  // If token initialization step fails (e.g. if tpm token is not yet ready)
  // schedules the initialization step retry attempt after a timeout.
  void RetryLater();

  // Cryptohome methods callbacks.
  void OnTpmIsEnabled(DBusMethodCallStatus call_status,
                      bool tpm_is_enabled);
  void OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                               const std::string& token_name,
                               const std::string& user_pin,
                               int token_slot_id);

  // The task runner used to run delayed tasks when retrying failed Cryptohome
  // calls.
  scoped_refptr<base::TaskRunner> delayed_task_runner_;

  Type type_;
  State state_;

  // The user id associated with the TPMTokenInfoGetter. Empty for system token.
  std::string user_id_;

  TPMTokenInfoCallback callback_;

  // The current request delay before the next attempt to initialize the
  // TPM. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  CryptohomeClient* cryptohome_client_;

  base::WeakPtrFactory<TPMTokenInfoGetter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TPMTokenInfoGetter);
};

}  // namespace chromeos

#endif  // CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_
