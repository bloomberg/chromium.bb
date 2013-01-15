// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// CryptohomeClient is used to communicate with the Cryptohome service.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT CryptohomeClient {
 public:
  // A callback to handle AsyncCallStatus signals.
  typedef base::Callback<void(int async_id,
                              bool return_status,
                              int return_code)>
      AsyncCallStatusHandler;
  // A callback to handle AsyncCallStatusWithData signals.
  typedef base::Callback<void(int async_id,
                              bool return_status,
                              const std::string& data)>
      AsyncCallStatusWithDataHandler;
  // A callback to handle responses of AsyncXXX methods.
  typedef base::Callback<void(int async_id)> AsyncMethodCallback;
  // A callback to handle responses of Pkcs11GetTpmTokenInfo method.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const std::string& label,
      const std::string& user_pin)> Pkcs11GetTpmTokenInfoCallback;

  virtual ~CryptohomeClient();

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CryptohomeClient* Create(DBusClientImplementationType type,
                                  dbus::Bus* bus);

  // Sets AsyncCallStatus signal handlers.
  // |handler| is called when results for AsyncXXX methods are returned.
  // Cryptohome service will process the calls in a first-in-first-out manner
  // when they are made in parallel.
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) = 0;

  // Resets AsyncCallStatus signal handlers.
  virtual void ResetAsyncCallStatusHandlers() = 0;

  // Calls IsMounted method and returns true when the call succeeds.
  virtual void IsMounted(const BoolDBusMethodCallback& callback) = 0;

  // Calls Unmount method and returns true when the call succeeds.
  // This method blocks until the call returns.
  virtual bool Unmount(bool* success) = 0;

  // Calls AsyncCheckKey method.  |callback| is called after the method call
  // succeeds.
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             const AsyncMethodCallback& callback) = 0;

  // Calls AsyncMigrateKey method.  |callback| is called after the method call
  // succeeds.
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               const AsyncMethodCallback& callback) = 0;

  // Calls AsyncRemove method.  |callback| is called after the method call
  // succeeds.
  virtual void AsyncRemove(const std::string& username,
                           const AsyncMethodCallback& callback) = 0;

  // Calls GetSystemSalt method.  This method blocks until the call returns.
  // The original content of |salt| is lost.
  virtual bool GetSystemSalt(std::vector<uint8>* salt) = 0;

  // Calls the AsyncMount method to asynchronously mount the cryptohome for
  // |username|, using |key| to unlock it. For supported |flags|, see the
  // documentation of AsyncMethodCaller::AsyncMount().
  // |callback| is called after the method call succeeds.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) = 0;

  // Calls AsyncMountGuest method.  |callback| is called after the method call
  // succeeds.
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) = 0;

  // Calls TpmIsReady method.
  virtual void TpmIsReady(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsEnabled method.
  virtual void TpmIsEnabled(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsEnabled method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crosbug.com/28500
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) = 0;

  // Calls TpmGetPassword method.
  virtual void TpmGetPassword(const StringDBusMethodCallback& callback) = 0;

  // Calls TpmIsOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  virtual bool TpmIsOwned(bool* owned) = 0;

  // Calls TpmIsBeingOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  virtual bool TpmIsBeingOwned(bool* owning) = 0;

  // Calls TpmCanAttemptOwnership method.
  // This method tells the service that it is OK to attempt ownership.
  virtual void TpmCanAttemptOwnership(
      const VoidDBusMethodCallback& callback) = 0;

  // Calls TpmClearStoredPassword method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool TpmClearStoredPassword() = 0;

  // Calls Pkcs11IsTpmTokenReady method.
  virtual void Pkcs11IsTpmTokenReady(
      const BoolDBusMethodCallback& callback) = 0;

  // Calls Pkcs11GetTpmTokenInfo method.
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) = 0;

  // Calls InstallAttributesGet method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // The original content of |value| is lost.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) = 0;

  // Calls InstallAttributesSet method and returns true when the call succeeds.
  // This method blocks until the call returns.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) = 0;

  // Calls InstallAttributesFinalize method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool InstallAttributesFinalize(bool* successful) = 0;

  // Calls InstallAttributesIsReady method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool InstallAttributesIsReady(bool* is_ready) = 0;

  // Calls InstallAttributesIsInvalid method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) = 0;

  // Calls InstallAttributesIsFirstInstall method and returns true when the call
  // succeeds. This method blocks until the call returns.
  virtual bool InstallAttributesIsFirstInstall(bool* is_first_install) = 0;

  // Calls the TpmAttestationIsPrepared dbus method.  The callback is called
  // when the operation completes.
  virtual void TpmAttestationIsPrepared(
        const BoolDBusMethodCallback& callback) = 0;

  // Calls the TpmAttestationIsEnrolled dbus method.  The callback is called
  // when the operation completes.
  virtual void TpmAttestationIsEnrolled(
        const BoolDBusMethodCallback& callback) = 0;

  // Asynchronously creates an attestation enrollment request.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusWithDataHandler signal handler is called.  The data that
  // is sent with the signal is an enrollment request to be sent to the Privacy
  // CA.  The enrollment is completed by calling AsyncTpmAttestationEnroll.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously finishes an attestation enrollment operation.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusHandler signal handler is called.  |pca_response| is the
  // response to the enrollment request emitted by the Privacy CA.
  virtual void AsyncTpmAttestationEnroll(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously creates an attestation certificate request.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusWithDataHandler signal handler is called.  The data that
  // is sent with the signal is a certificate request to be sent to the Privacy
  // CA.  The certificate request is completed by calling
  // AsyncTpmAttestationFinishCertRequest.
  virtual void AsyncTpmAttestationCreateCertRequest(
      bool is_cert_for_owner,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously finishes a certificate request operation.  The callback will
  // be called when the dbus call completes.  When the operation completes, the
  // AsyncCallStatusWithDataHandler signal handler is called.  The data that is
  // sent with the signal is a certificate chain in PEM format.  |pca_response|
  // is the response to the certificate request emitted by the Privacy CA.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  CryptohomeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_
