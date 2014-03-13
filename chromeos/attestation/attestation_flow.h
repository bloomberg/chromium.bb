// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

class AsyncMethodCaller;

}  // namespace cryptohome

namespace chromeos {

class CryptohomeClient;

namespace attestation {

// Interface for access to the Privacy CA server.
class CHROMEOS_EXPORT ServerProxy {
 public:
  typedef base::Callback<void(bool success,
                              const std::string& data)> DataCallback;
  virtual ~ServerProxy();
  virtual void SendEnrollRequest(const std::string& request,
                                 const DataCallback& on_response) = 0;
  virtual void SendCertificateRequest(const std::string& request,
                                      const DataCallback& on_response) = 0;
  virtual PrivacyCAType GetType();
};

// Implements the message flow for Chrome OS attestation tasks.  Generally this
// consists of coordinating messages between the Chrome OS attestation service
// and the Chrome OS Privacy CA server.  Sample usage:
//    AttestationFlow flow(AsyncMethodCaller::GetInstance(),
//                         DBusThreadManager::Get().GetCryptohomeClient(),
//                         my_server_proxy.Pass());
//    AttestationFlow::CertificateCallback callback = base::Bind(&MyCallback);
//    flow.GetCertificate(ENTERPRISE_USER_CERTIFICATE, false, callback);
class CHROMEOS_EXPORT AttestationFlow {
 public:
  typedef base::Callback<void(bool success,
                              const std::string& pem_certificate_chain)>
      CertificateCallback;

  AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                  CryptohomeClient* cryptohome_client,
                  scoped_ptr<ServerProxy> server_proxy);
  virtual ~AttestationFlow();

  // Gets an attestation certificate for a hardware-protected key.  If a key for
  // the given profile does not exist, it will be generated and a certificate
  // request will be made to the Chrome OS Privacy CA to issue a certificate for
  // the key.  If the key already exists and |force_new_key| is false, the
  // existing certificate is returned.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   user_id - Identifies the currently active user.  For normal GAIA users
  //             this is a canonical email address.  This is ignored when using
  //             the enterprise machine cert profile.
  //   request_origin - For content protection profiles, certificate requests
  //                    are origin-specific.  This string must uniquely identify
  //                    the origin of the request.
  //   force_new_key - If set to true, a new key will be generated even if a key
  //                   already exists for the profile.  The new key will replace
  //                   the existing key on success.
  //   callback - A callback which will be called when the operation completes.
  //              On success |result| will be true and |data| will contain the
  //              PCA-issued certificate chain in PEM format.
  virtual void GetCertificate(AttestationCertificateProfile certificate_profile,
                              const std::string& user_id,
                              const std::string& request_origin,
                              bool force_new_key,
                              const CertificateCallback& callback);

 private:
  // Asynchronously initiates the attestation enrollment flow.
  //
  // Parameters
  //   on_failure - Called if any failure occurs.
  //   next_task - Called on successful enrollment.
  void StartEnroll(const base::Closure& on_failure,
                   const base::Closure& next_task);

  // Called when the attestation daemon has finished creating an enrollment
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   on_failure - Called if any failure occurs.
  //   next_task - Called on successful enrollment.
  //   success - The status of request creation.
  //   data - The request data for the Privacy CA.
  void SendEnrollRequestToPCA(const base::Closure& on_failure,
                              const base::Closure& next_task,
                              bool success,
                              const std::string& data);

  // Called when the Privacy CA responds to an enrollment request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the enrollment operation.
  //
  // Parameters
  //   on_failure - Called if any failure occurs.
  //   next_task - Called on successful enrollment.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendEnrollResponseToDaemon(const base::Closure& on_failure,
                                  const base::Closure& next_task,
                                  bool success,
                                  const std::string& data);

  // Called when the attestation daemon completes an enrollment operation.  If
  // the operation was successful, the next_task callback is called.
  //
  // Parameters
  //   on_failure - Called if any failure occurs.
  //   next_task - Called on successful enrollment.
  //   success - The status of the enrollment operation.
  //   not_used - An artifact of the cryptohome D-Bus interface; ignored.
  void OnEnrollComplete(const base::Closure& on_failure,
                        const base::Closure& next_task,
                        bool success,
                        cryptohome::MountError not_used);

  // Asynchronously initiates the certificate request flow.  Attestation
  // enrollment must complete successfully before this operation can succeed.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   user_id - Identifies the active user.
  //   request_origin - An identifier for the origin of this request.
  //   generate_new_key - If set to true a new key is generated.
  //   callback - Called when the operation completes.
  void StartCertificateRequest(
      const AttestationCertificateProfile certificate_profile,
      const std::string& user_id,
      const std::string& request_origin,
      bool generate_new_key,
      const CertificateCallback& callback);

  // Called when the attestation daemon has finished creating a certificate
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   user_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of request creation.
  //   data - The request data for the Privacy CA.
  void SendCertificateRequestToPCA(AttestationKeyType key_type,
                                   const std::string& user_id,
                                   const std::string& key_name,
                                   const CertificateCallback& callback,
                                   bool success,
                                   const std::string& data);

  // Called when the Privacy CA responds to a certificate request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the operation.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   user_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendCertificateResponseToDaemon(AttestationKeyType key_type,
                                       const std::string& user_id,
                                       const std::string& key_name,
                                       const CertificateCallback& callback,
                                       bool success,
                                       const std::string& data);

  // Gets an existing certificate from the attestation daemon.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   user_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  void GetExistingCertificate(AttestationKeyType key_type,
                              const std::string& user_id,
                              const std::string& key_name,
                              const CertificateCallback& callback);

  cryptohome::AsyncMethodCaller* async_caller_;
  CryptohomeClient* cryptohome_client_;
  scoped_ptr<ServerProxy> server_proxy_;

  base::WeakPtrFactory<AttestationFlow> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AttestationFlow);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
