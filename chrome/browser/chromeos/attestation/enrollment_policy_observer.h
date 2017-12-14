// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class CrosSettings;
class CryptohomeClient;

namespace attestation {

class AttestationFlow;

// A class which observes policy changes and triggers uploading identification
// for enrollment if necessary.
class EnrollmentPolicyObserver {
 public:
  // The observer immediately connects with CrosSettings to listen for policy
  // changes.  The CloudPolicyClient is used to upload data to the server; it
  // must be in the registered state.  This class does not take ownership of
  // |policy_client|.
  explicit EnrollmentPolicyObserver(policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom CryptohomeClient and AttestationFlow
  // implementations.  Useful for testing.
  EnrollmentPolicyObserver(policy::CloudPolicyClient* policy_client,
                           CryptohomeClient* cryptohome_client,
                           AttestationFlow* attestation_flow);

  ~EnrollmentPolicyObserver();

  // Sets the retry delay in seconds; useful in testing.
  void set_retry_delay(int retry_delay) { retry_delay_ = retry_delay; }

 private:
  // Called when the enrollment setting changes.
  void EnrollmentSettingChanged();

  // Checks enrollment setting and starts any necessary work.
  void Start();

  // Gets an enrollment certificate.
  void GetEnrollmentCertificate();

  // Uploads an enrollment certificate to the policy server.
  void UploadCertificate(const std::string& pem_certificate_chain);

  // Called when a certificate upload operation completes.  On success, |status|
  // will be true.
  void OnUploadComplete(bool status);

  // Reschedules a policy check (i.e. a call to Start) for a later time.
  // TODO(crbug.com/256845): A better solution would be to wait for a DBUS
  // signal which indicates the system is ready to process this task.
  void Reschedule();

  CrosSettings* cros_settings_;
  policy::CloudPolicyClient* policy_client_;
  CryptohomeClient* cryptohome_client_;
  AttestationFlow* attestation_flow_;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  int num_retries_;
  int retry_delay_;

  std::unique_ptr<CrosSettings::ObserverSubscription> attestation_subscription_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EnrollmentPolicyObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentPolicyObserver);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_
