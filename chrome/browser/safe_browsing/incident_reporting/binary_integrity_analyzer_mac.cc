// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_mac.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/signature_evaluator_mac.h"
#include "chrome/common/safe_browsing/csd.pb.h"

#define DEVELOPER_ID_APPLICATION_OID "field.1.2.840.113635.100.6.1.13"
#define DEVELOPER_ID_INTERMEDIATE_OID "field.1.2.840.113635.100.6.2.6"

namespace safe_browsing {

namespace {

void VerifyBinaryIntegrityHelper(IncidentReceiver* incident_receiver,
                                 const base::FilePath& path,
                                 const std::string& requirement) {
  MacSignatureEvaluator evaluator(path, requirement);
  if (!evaluator.Initialize()) {
    LOG(ERROR) << "Could not initialize mac signature evaluator";
    return;
  }

  std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
      incident(new ClientIncidentReport_IncidentData_BinaryIntegrityIncident());
  if (!evaluator.PerformEvaluation(incident.get())) {
    incident_receiver->AddIncidentForProcess(
        base::MakeUnique<BinaryIntegrityIncident>(std::move(incident)));
  } else {
    // Clear past incidents involving this bundle if the signature is
    // now valid.
    ClearBinaryIntegrityForFile(incident_receiver, path.BaseName().value());
  }
}

}  // namespace

std::vector<PathAndRequirement> GetCriticalPathsAndRequirements() {
  // Get the path to the main executable.
  std::vector<PathAndRequirement> critical_binaries;
  // This requirement describes a developer ID signed application,
  // with Google's team identifier, and the com.Google.Chrome[.canary]
  // identifier.
  std::string requirement =
      "anchor apple generic and certificate 1[" DEVELOPER_ID_INTERMEDIATE_OID
      "] exists and certificate leaf[" DEVELOPER_ID_APPLICATION_OID
      "] exists and certificate leaf[subject.OU]=\"EQHXZ8M8AV\" and "
      "(identifier=\"com.google.Chrome\" or "
      "identifier=\"com.google.Chrome.canary\")";
  critical_binaries.push_back(
      PathAndRequirement(base::mac::OuterBundlePath(), requirement));
  // TODO(kerrnel): eventually add Adobe Flash Player to this list.
  return critical_binaries;
}

void VerifyBinaryIntegrityForTesting(IncidentReceiver* incident_receiver,
                                     const base::FilePath& path,
                                     const std::string& requirement) {
  VerifyBinaryIntegrityHelper(incident_receiver, path, requirement);
}

void VerifyBinaryIntegrity(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  size_t i = 0;
  for (const auto& p : GetCriticalPathsAndRequirements()) {
    base::TimeTicks time_before = base::TimeTicks::Now();
    VerifyBinaryIntegrityHelper(incident_receiver.get(), p.path, p.requirement);
    RecordSignatureVerificationTime(i++, base::TimeTicks::Now() - time_before);
  }
}

}  // namespace
