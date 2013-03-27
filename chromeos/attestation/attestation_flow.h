// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
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
  virtual ~ServerProxy() {}
  virtual void SendEnrollRequest(const std::string& request,
                                 const DataCallback& on_response) = 0;
  virtual void SendCertificateRequest(const std::string& request,
                                      const DataCallback& on_response) = 0;
};

// Implements the message flow for Chrome OS attestation tasks.  Generally this
// consists of coordinating messages between the Chrome OS attestation service
// and the Privacy CA server.  Sample usage:
//    AttestationFlow flow(AsyncMethodCaller::GetInstance(),
//                         DBusThreadManager::Get().GetCryptohomeClient(),
//                         my_server_proxy);
//    CertificateCallback callback = base::Bind(&MyCallback);
//    flow.GetCertificate("attest-ent-machine", callback);
class CHROMEOS_EXPORT AttestationFlow {
 public:
  typedef base::Callback<void(bool success,
                              const std::string& pem_certificate_chain)>
      CertificateCallback;

  AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                  CryptohomeClient* cryptohome_client,
                  ServerProxy* server_proxy);
  virtual ~AttestationFlow();

  // Asynchronously gets an attestation certificate bound to the given name.
  // If no certificate has been associated with the name, a new certificate is
  // issued.
  //
  // Parameters
  //   name - The name of the key for which to retrieve a certificate.  The
  //          following key names are available:
  //            "attest-ent-machine" - The enterprise machine key.
  //            "attest-ent-user" - An enterprise user key for the current user.
  //            "content-[origin]" - A content protection key bound to a
  //                                 specific origin for the current user.
  //   callback - A callback which will be called when the operation completes.
  virtual void GetCertificate(const std::string& name,
                              const CertificateCallback& callback);

 private:
  // The key name defined for the special-purpose Enterprise Machine Key.
  static const char kEnterpriseMachineKey[];

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
  //   name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  void StartCertificateRequest(const std::string& name,
                               const CertificateCallback& callback);

  // Called when the attestation daemon has finished creating a certificate
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of request creation.
  //   data - The request data for the Privacy CA.
  void SendCertificateRequestToPCA(const std::string& name,
                                   const CertificateCallback& callback,
                                   bool success,
                                   const std::string& data);

  // Called when the Privacy CA responds to a certificate request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the operation.
  //
  // Parameters
  //   name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendCertificateResponseToDaemon(const std::string& name,
                                       const CertificateCallback& callback,
                                       bool success,
                                       const std::string& data);

  base::WeakPtrFactory<AttestationFlow> weak_factory_;
  cryptohome::AsyncMethodCaller* async_caller_;
  CryptohomeClient* cryptohome_client_;
  ServerProxy* server_proxy_;

  DISALLOW_COPY_AND_ASSIGN(AttestationFlow);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
