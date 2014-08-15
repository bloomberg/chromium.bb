// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/binary_feature_extractor.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

namespace {

void RecordSignatureVerificationTime(size_t file_index,
    const base::TimeDelta& verification_time) {
  static const char kHistogramName[] = "SBIRS.VerifyBinaryIntegrity.";

  base::HistogramBase* signature_verification_time_histogram =
      base::Histogram::FactoryTimeGet(
          std::string(kHistogramName) + base::IntToString(file_index),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(20),
          50,
          base::Histogram::kUmaTargetedHistogramFlag);

    signature_verification_time_histogram->AddTime(verification_time);
}

}  // namespace

void RegisterBinaryIntegrityAnalysis() {
#if defined(OS_WIN)
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyBinaryIntegrity));
#endif
}

void VerifyBinaryIntegrity(const AddIncidentCallback& callback) {
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());

  std::vector<base::FilePath> critical_binaries = GetCriticalBinariesPath();
  for (size_t i = 0; i < critical_binaries.size(); ++i) {
    base::FilePath binary_path(critical_binaries[i]);
    if (!base::PathExists(binary_path))
      continue;

    scoped_ptr<ClientDownloadRequest_SignatureInfo> signature_info(
        new ClientDownloadRequest_SignatureInfo());

    base::TimeTicks time_before = base::TimeTicks::Now();
    binary_feature_extractor->CheckSignature(binary_path, signature_info.get());
    RecordSignatureVerificationTime(i, base::TimeTicks::Now() - time_before);

    // Only create a report if the signature is untrusted.
    if (!signature_info->trusted()) {
      scoped_ptr<ClientIncidentReport_IncidentData> incident_data(
          new ClientIncidentReport_IncidentData());
      ClientIncidentReport_IncidentData_BinaryIntegrityIncident*
          binary_integrity = incident_data->mutable_binary_integrity();

      binary_integrity->set_file_basename(
          binary_path.BaseName().AsUTF8Unsafe());
      binary_integrity->set_allocated_signature(signature_info.release());

      // Send the report.
      callback.Run(incident_data.Pass());
    }
  }
}

#if !defined(OS_WIN)
std::vector<base::FilePath> GetCriticalBinariesPath() {
  return std::vector<base::FilePath>();
}
#endif  // !defined(OS_WIN)

}  // namespace safe_browsing
