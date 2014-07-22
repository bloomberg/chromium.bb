// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_TOKEN_LOADER_H_
#define CHROMEOS_TPM_TOKEN_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/login_state.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

// This class is responsible for loading the TPM backed token for the system
// slot when the user logs in. It is expected to be constructed on the UI thread
// and public methods should all be called from the UI thread.
// When the TPM token is loaded, or if the TPM should stay disabled for the
// session, the observers are notified using |OnTPMTokenReady|.
// Note: This currently initializes the token with the hard coded default id 0.
// See CryptohomeClient::OnPkcs11GetTpmTokenInfo.
class CHROMEOS_EXPORT TPMTokenLoader : public LoginState::Observer {
 public:
  class Observer {
   public:
    // Called when the TPM token initialization is done or the case where TPM
    // should stay disabled is detected (e.g. on guest login).
    virtual void OnTPMTokenReady() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Sets the global instance. Must be called before any calls to Get().
  // The global instance will immediately start observing |LoginState|.
  static void Initialize();

  // Sets the global. stubbed out, instance. To be used in tests.
  static void InitializeForTest();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called before this.
  static TPMTokenLoader* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // |crypto_task_runner| is the task runner that any synchronous crypto calls
  // should be made from, e.g. in Chrome this is the IO thread. Must be called
  // after the thread is started. When called, this will attempt to start TPM
  // token loading.
  void SetCryptoTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner);

  void AddObserver(TPMTokenLoader::Observer* observer);
  void RemoveObserver(TPMTokenLoader::Observer* observer);

  // Checks if the TPM token in ready to be used.
  bool IsTPMTokenReady() const;

  std::string tpm_user_pin() const { return tpm_user_pin_; }

 private:
  explicit TPMTokenLoader(bool for_test);
  virtual ~TPMTokenLoader();

  // Starts tpm token initialization if the user is logged in and the crypto
  // task runner is set.
  void MaybeStartTokenInitialization();

  // This is the cyclic chain of callbacks to initialize the TPM token.
  void ContinueTokenInitialization();
  void OnTPMTokenEnabledForNSS();
  void OnTpmIsEnabled(DBusMethodCallStatus call_status,
                      bool tpm_is_enabled);
  void OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                               bool is_tpm_token_ready);
  void OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                               const std::string& token_name,
                               const std::string& user_pin,
                               int token_slot_id);
  void OnTPMTokenInitialized(bool success);

  // If token initialization step fails (e.g. if tpm token is not yet ready)
  // schedules the initialization step retry attempt after a timeout.
  void RetryTokenInitializationLater();

  // Notifies observers that the TPM token is ready.
  void NotifyTPMTokenReady();

  // LoginState::Observer
  virtual void LoggedInStateChanged() OVERRIDE;

  bool initialized_for_test_;

  ObserverList<Observer> observers_;

  // The states are traversed in this order but some might get omitted or never
  // be left.
  enum TPMTokenState {
    TPM_STATE_UNKNOWN,
    TPM_INITIALIZATION_STARTED,
    TPM_TOKEN_ENABLED_FOR_NSS,
    TPM_DISABLED,
    TPM_ENABLED,
    TPM_TOKEN_READY,
    TPM_TOKEN_INFO_RECEIVED,
    TPM_TOKEN_INITIALIZED,
  };
  TPMTokenState tpm_token_state_;

  // The current request delay before the next attempt to initialize the
  // TPM. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  // Cached TPM token info.
  int tpm_token_slot_id_;
  std::string tpm_user_pin_;

  base::ThreadChecker thread_checker_;

  // TaskRunner for crypto calls.
  scoped_refptr<base::SequencedTaskRunner> crypto_task_runner_;

  base::WeakPtrFactory<TPMTokenLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TPMTokenLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_TPM_TOKEN_LOADER_H_
