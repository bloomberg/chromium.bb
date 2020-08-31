// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"

namespace chromeos {
namespace cert_provisioning {

// The enum is used for UMA, the values should not be renumerated.
enum class CertProvisioningEvent {
  // Some worker tried to register(or reregister) for invalidation topic.
  kRegisteredToInvalidationTopic = 0,
  // Invalidation received.
  kInvalidationReceived = 1,
  // Some worker retried to continue without invalidation.
  kWorkerRetryWithoutInvalidation = 2,
  // Some worker retried to continue without invalidation and made some
  // progress.
  kWorkerRetrySucceededWithoutInvalidation = 3,
  // Profile retried manually from UI.
  kWorkerRetryManual = 4,
  kWorkerCreated = 5,
  kWorkerDeserialized = 6,
  kWorkerDeserializationFailed = 7,
  kMaxValue = kWorkerDeserializationFailed
};

// Records the |final_state| of a worker. If the worker is failed, also records
// its |prev_state| into the same histogram. It is reasonable to put both of
// them in the same histogram because the worker should never stop on an
// intermediate state and even if it does, it is the same as failure.
void RecordResult(CertScope scope,
                  CertProvisioningWorkerState final_state,
                  CertProvisioningWorkerState prev_state);

void RecordEvent(CertScope scope, CertProvisioningEvent event);

// Records time of generation key pair by certificate provisioning worker.
void RecordKeypairGenerationTime(CertScope scope, base::TimeDelta sample);

// Records time of building Verified Access response by certificate provisioning
// worker.
void RecordVerifiedAccessTime(CertScope scope, base::TimeDelta sample);

// Records time of signing a CSR by certificate provisioning worker.
void RecordCsrSignTime(CertScope scope, base::TimeDelta sample);

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_METRICS_H_
