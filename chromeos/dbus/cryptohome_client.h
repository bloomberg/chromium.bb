// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace cryptohome {

class AccountIdentifier;
class AddKeyRequest;
class AuthorizationRequest;
class BaseReply;
class CheckKeyRequest;
class FlushAndSignBootAttributesRequest;
class GetBootAttributeRequest;
class MountRequest;
class RemoveKeyRequest;
class SetBootAttributeRequest;
class UpdateKeyRequest;

} // namespace cryptohome

namespace chromeos {

// CryptohomeClient is used to communicate with the Cryptohome service.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT CryptohomeClient : public DBusClient {
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
  // A callback for GetSystemSalt().
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const std::vector<uint8>& system_salt)> GetSystemSaltCallback;
  // A callback for WaitForServiceToBeAvailable().
  typedef base::Callback<void(bool service_is_ready)>
      WaitForServiceToBeAvailableCallback;
  // A callback to handle responses of Pkcs11GetTpmTokenInfo method.  The result
  // of the D-Bus call is in |call_status|.  On success, |label| holds the
  // PKCS #11 token label.  This is not useful in practice to identify a token
  // but may be meaningful to a user.  The |user_pin| can be used with the
  // C_Login PKCS #11 function but is not necessary because tokens are logged in
  // for the duration of a signed-in session.  The |slot| corresponds to a
  // CK_SLOT_ID for the PKCS #11 API and reliably identifies the token for the
  // duration of the signed-in session.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const std::string& label,
      const std::string& user_pin,
      int slot)> Pkcs11GetTpmTokenInfoCallback;
  // A callback for methods which return both a bool result and data.
  typedef base::Callback<void(DBusMethodCallStatus call_status,
                              bool result,
                              const std::string& data)> DataMethodCallback;

  // A callback for methods which return both a bool and a protobuf as reply.
  typedef base::Callback<
      void(DBusMethodCallStatus call_status,
           bool result,
           const cryptohome::BaseReply& reply)> ProtobufMethodCallback;

  virtual ~CryptohomeClient();

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CryptohomeClient* Create();

  // Returns the sanitized |username| that the stub implementation would return.
  static std::string GetStubSanitizedUsername(const std::string& username);

  // Sets AsyncCallStatus signal handlers.
  // |handler| is called when results for AsyncXXX methods are returned.
  // Cryptohome service will process the calls in a first-in-first-out manner
  // when they are made in parallel.
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) = 0;

  // Resets AsyncCallStatus signal handlers.
  virtual void ResetAsyncCallStatusHandlers() = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      const WaitForServiceToBeAvailableCallback& callback) = 0;

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

  // Calls GetSystemSalt method.  |callback| is called after the method call
  // succeeds.
  virtual void GetSystemSalt(const GetSystemSaltCallback& callback) = 0;

  // Calls GetSanitizedUsername method.  |callback| is called after the method
  // call succeeds.
  virtual void GetSanitizedUsername(
      const std::string& username,
      const StringDBusMethodCallback& callback) = 0;

  // Same as GetSanitizedUsername() but blocks until a reply is received, and
  // returns the sanitized username synchronously. Returns an empty string if
  // the method call fails.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  virtual std::string BlockingGetSanitizedUsername(
      const std::string& username) = 0;

  // Calls the AsyncMount method to asynchronously mount the cryptohome for
  // |username|, using |key| to unlock it. For supported |flags|, see the
  // documentation of AsyncMethodCaller::AsyncMount().
  // |callback| is called after the method call succeeds.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) = 0;

  // Calls the AsyncAddKey method to asynchronously add another |new_key| for
  // |username|, using |key| to unlock it first.
  // |callback| is called after the method call succeeds.
  virtual void AsyncAddKey(const std::string& username,
                           const std::string& key,
                           const std::string& new_key,
                           const AsyncMethodCallback& callback) = 0;

  // Calls AsyncMountGuest method.  |callback| is called after the method call
  // succeeds.
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) = 0;

  // Calls the AsyncMount method to asynchronously mount the cryptohome for
  // |public_mount_id|. For supported |flags|, see the documentation of
  // AsyncMethodCaller::AsyncMount().  |callback| is called after the method
  // call succeeds.
  virtual void AsyncMountPublic(const std::string& public_mount_id,
                                int flags,
                                const AsyncMethodCallback& callback) = 0;

  // Calls TpmIsReady method.
  virtual void TpmIsReady(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsEnabled method.
  virtual void TpmIsEnabled(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsEnabled method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141006
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) = 0;

  // Calls TpmGetPassword method.
  virtual void TpmGetPassword(const StringDBusMethodCallback& callback) = 0;

  // Calls TpmIsOwned method.
  virtual void TpmIsOwned(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141012
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) = 0;

  // Calls TpmIsBeingOwned method.
  virtual void TpmIsBeingOwned(const BoolDBusMethodCallback& callback) = 0;

  // Calls TpmIsBeingOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141011
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) = 0;

  // Calls TpmCanAttemptOwnership method.
  // This method tells the service that it is OK to attempt ownership.
  virtual void TpmCanAttemptOwnership(
      const VoidDBusMethodCallback& callback) = 0;

  // Calls TpmClearStoredPasswordMethod.
  virtual void TpmClearStoredPassword(
      const VoidDBusMethodCallback& callback) = 0;

  // Calls TpmClearStoredPassword method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141010
  virtual bool CallTpmClearStoredPasswordAndBlock() = 0;

  // Calls Pkcs11IsTpmTokenReady method.
  virtual void Pkcs11IsTpmTokenReady(
      const BoolDBusMethodCallback& callback) = 0;

  // Calls Pkcs11GetTpmTokenInfo method.  This method is deprecated, you should
  // use Pkcs11GetTpmTokenInfoForUser instead.  On success |callback| will
  // receive PKCS #11 token information for the token associated with the user
  // who originally signed in (i.e. PKCS #11 slot 0).
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) = 0;

  // Calls Pkcs11GetTpmTokenInfoForUser method.  On success |callback| will
  // receive PKCS #11 token information for the user identified by |user_email|.
  // The |user_email| must be a canonical email address as returned by
  // user_manager::User::email().
  virtual void Pkcs11GetTpmTokenInfoForUser(
      const std::string& user_email,
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

  // Calls InstallAttributesIsReady method.
  virtual void InstallAttributesIsReady(
      const BoolDBusMethodCallback& callback) = 0;

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
  // CA of type |pca_type|.  The enrollment is completed by calling
  // AsyncTpmAttestationEnroll.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously finishes an attestation enrollment operation.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusHandler signal handler is called.  |pca_response| is the
  // response to the enrollment request emitted by the Privacy CA of type
  // |pca_type|.
  virtual void AsyncTpmAttestationEnroll(
      chromeos::attestation::PrivacyCAType pca_type,
      const std::string& pca_response,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously creates an attestation certificate request according to
  // |certificate_profile|.  Some profiles require that the |user_id| of the
  // currently active user and an identifier of the |request_origin| be
  // provided.  |callback| will be called when the dbus call completes.  When
  // the operation completes, the AsyncCallStatusWithDataHandler signal handler
  // is called.  The data that is sent with the signal is a certificate request
  // to be sent to the Privacy CA of type |pca_type|.  The certificate request
  // is completed by calling AsyncTpmAttestationFinishCertRequest.  The
  // |user_id| will not be included in the certificate request for the Privacy
  // CA.
  virtual void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const std::string& user_id,
      const std::string& request_origin,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously finishes a certificate request operation.  The callback will
  // be called when the dbus call completes.  When the operation completes, the
  // AsyncCallStatusWithDataHandler signal handler is called.  The data that is
  // sent with the signal is a certificate chain in PEM format.  |pca_response|
  // is the response to the certificate request emitted by the Privacy CA.
  // |key_type| determines whether the certified key is to be associated with
  // the current user.  |key_name| is a name for the key.  If |key_type| is
  // KEY_USER, a |user_id| must be provided.  Otherwise |user_id| is ignored.
  // For normal GAIA users the |user_id| is a canonical email address.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const AsyncMethodCallback& callback) = 0;

  // Checks if an attestation key already exists.  If the key specified by
  // |key_type| and |key_name| exists, then the result sent to the callback will
  // be true.  If |key_type| is KEY_USER, a |user_id| must be provided.
  // Otherwise |user_id| is ignored.  For normal GAIA users the |user_id| is a
  // canonical email address.
  virtual void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) = 0;

  // Gets the attestation certificate for the key specified by |key_type| and
  // |key_name|.  |callback| will be called when the operation completes.  If
  // the key does not exist the callback |result| parameter will be false.  If
  // |key_type| is KEY_USER, a |user_id| must be provided.  Otherwise |user_id|
  // is ignored.  For normal GAIA users the |user_id| is a canonical email
  // address.
  virtual void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) = 0;

  // Gets the public key for the key specified by |key_type| and |key_name|.
  // |callback| will be called when the operation completes.  If the key does
  // not exist the callback |result| parameter will be false.  If |key_type| is
  // KEY_USER, a |user_id| must be provided.  Otherwise |user_id| is ignored.
  // For normal GAIA users the |user_id| is a canonical email address.
  virtual void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) = 0;

  // Asynchronously registers an attestation key with the current user's
  // PKCS #11 token.  The |callback| will be called when the dbus call
  // completes.  When the operation completes, the AsyncCallStatusHandler signal
  // handler is called.  |key_type| and |key_name| specify the key to register.
  // If |key_type| is KEY_USER, a |user_id| must be provided.  Otherwise
  // |user_id| is ignored.  For normal GAIA users the |user_id| is a canonical
  // email address.
  virtual void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously signs an enterprise challenge with the key specified by
  // |key_type| and |key_name|.  |domain| and |device_id| will be included in
  // the challenge response.  |options| control how the challenge response is
  // generated.  |challenge| must be a valid enterprise attestation challenge.
  // The |callback| will be called when the dbus call completes.  When the
  // operation completes, the AsyncCallStatusWithDataHandler signal handler is
  // called.  If |key_type| is KEY_USER, a |user_id| must be provided.
  // Otherwise |user_id| is ignored.  For normal GAIA users the |user_id| is a
  // canonical email address.
  virtual void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const AsyncMethodCallback& callback) = 0;

  // Asynchronously signs a simple challenge with the key specified by
  // |key_type| and |key_name|.  |challenge| can be any set of arbitrary bytes.
  // A nonce will be appended to the challenge before signing; this method
  // cannot be used to sign arbitrary data.  The |callback| will be called when
  // the dbus call completes.  When the operation completes, the
  // AsyncCallStatusWithDataHandler signal handler is called.  If |key_type| is
  // KEY_USER, a |user_id| must be provided.  Otherwise |user_id| is ignored.
  // For normal GAIA users the |user_id| is a canonical email address.
  virtual void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) = 0;

  // Gets the payload associated with the key specified by |key_type| and
  // |key_name|.  The |callback| will be called when the operation completes.
  // If the key does not exist the callback |result| parameter will be false.
  // If no payload has been set for the key the callback |result| parameter will
  // be true and the |data| parameter will be empty.  If |key_type| is
  // KEY_USER, a |user_id| must be provided.  Otherwise |user_id| is ignored.
  // For normal GAIA users the |user_id| is a canonical email address.
  virtual void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) = 0;

  // Sets the |payload| associated with the key specified by |key_type| and
  // |key_name|.  The |callback| will be called when the operation completes.
  // If the operation succeeds, the callback |result| parameter will be true.
  // If |key_type| is KEY_USER, a |user_id| must be provided.  Otherwise
  // |user_id| is ignored.  For normal GAIA users the |user_id| is a canonical
  // email address.
  virtual void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& payload,
      const BoolDBusMethodCallback& callback) = 0;

  // Deletes certified keys as specified by |key_type| and |key_prefix|.  The
  // |callback| will be called when the operation completes.  If the operation
  // succeeds, the callback |result| parameter will be true.  If |key_type| is
  // KEY_USER, a |user_id| must be provided.  Otherwise |user_id| is ignored.
  // For normal GAIA users the |user_id| is a canonical email address.  All keys
  // where the key name has a prefix matching |key_prefix| will be deleted.  All
  // meta-data associated with the key, including certificates, will also be
  // deleted.
  virtual void TpmAttestationDeleteKeys(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_prefix,
      const BoolDBusMethodCallback& callback) = 0;

  // Asynchronously calls CheckKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // CheckKeyEx just checks if authorization information is valid.
  virtual void CheckKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::CheckKeyRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls MountEx method. |callback| is called after method
  // call, and with reply protobuf.
  // MountEx attempts to mount home dir using given authorization, and can
  // create new home dir if necessary values are specified in |request|.
  virtual void MountEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::MountRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls AddKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // AddKeyEx adds another key to the given key set. |request| also defines
  // behavior in case when key with specified label already exist.
  virtual void AddKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::AddKeyRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls UpdateKeyEx method. |callback| is called after method
  // call, and with reply protobuf. Reply will contain MountReply extension.
  // UpdateKeyEx replaces key used for authorization, without affecting any
  // other keys. If specified at home dir creation time, new key may have
  // to be signed and/or encrypted.
  virtual void UpdateKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::UpdateKeyRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls RemoveKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // RemoveKeyEx removes key from the given key set.
  virtual void RemoveKeyEx(const cryptohome::AccountIdentifier& id,
                           const cryptohome::AuthorizationRequest& auth,
                           const cryptohome::RemoveKeyRequest& request,
                           const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls GetBootAttribute method. |callback| is called after
  // method call, and with reply protobuf.
  // GetBootAttribute gets the value of the specified boot attribute.
  virtual void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls SetBootAttribute method. |callback| is called after
  // method call, and with reply protobuf.
  // SetBootAttribute sets the value of the specified boot attribute. The value
  // won't be available unitl FlushAndSignBootAttributes() is called.
  virtual void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      const ProtobufMethodCallback& callback) = 0;

  // Asynchronously calls FlushAndSignBootAttributes method. |callback| is
  // called after method call, and with reply protobuf.
  // FlushAndSignBootAttributes makes all pending boot attribute settings
  // available, and have them signed by a special TPM key. This method always
  // fails after any user, publuc, or guest session starts.
  virtual void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      const ProtobufMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  CryptohomeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_CLIENT_H_
