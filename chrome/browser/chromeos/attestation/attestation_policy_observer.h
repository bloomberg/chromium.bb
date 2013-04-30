// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class CrosSettings;
class CryptohomeClient;

namespace attestation {

class AttestationFlow;

// A class which observes policy changes and triggers device attestation work if
// necessary.
class AttestationPolicyObserver : public content::NotificationObserver {
 public:
  // The observer immediately connects with CrosSettings to listen for policy
  // changes.  The CloudPolicyClient is used to upload the device certificate to
  // the server if one is created in response to policy changes; it must be in
  // the registered state.  This class does not take ownership of
  // |policy_client|.
  explicit AttestationPolicyObserver(policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom CryptohomeClient and AttestationFlow
  // implementations.  Useful for testing.
  AttestationPolicyObserver(policy::CloudPolicyClient* policy_client,
                            CryptohomeClient* cryptohome_client,
                            AttestationFlow* attestation_flow);

  virtual ~AttestationPolicyObserver();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Checks attestation policy and starts any necessary work.
  void Start();

  // Gets a new certificate for the Enterprise Machine Key (EMK).
  void GetNewCertificate();

  // Gets the existing EMK certificate and sends it to CheckCertificateExpiry.
  void GetExistingCertificate();

  // Checks if the given certificate is expired and, if so, get a new one.
  void CheckCertificateExpiry(const std::string& certificate);

  // Uploads a certificate to the policy server.
  void UploadCertificate(const std::string& certificate);

  // Checks if a certificate has already been uploaded and, if not, upload.
  void CheckIfUploaded(const std::string& certificate,
                       const std::string& key_payload);

  // Gets the payload associated with the EMK and sends it to |callback|.
  void GetKeyPayload(base::Callback<void(const std::string&)> callback);

  // Called when a certificate upload operation completes.  On success, |status|
  // will be true.
  void OnUploadComplete(bool status);

  // Marks a key as uploaded in the payload proto.
  void MarkAsUploaded(const std::string& key_payload);

  CrosSettings* cros_settings_;
  policy::CloudPolicyClient* policy_client_;
  CryptohomeClient* cryptohome_client_;
  AttestationFlow* attestation_flow_;
  scoped_ptr<AttestationFlow> default_attestation_flow_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AttestationPolicyObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AttestationPolicyObserver);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_
