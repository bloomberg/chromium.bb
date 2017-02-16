// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_load_analyzer.h"

#include <set>
#include <utility>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/suspicious_module_incident.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/local_database_manager.h"
#elif defined(SAFE_BROWSING_DB_REMOTE)
#include "chrome/browser/safe_browsing/remote_database_manager.h"
#endif

namespace safe_browsing {

namespace {

void ReportIncidentsForSuspiciousModules(
    std::unique_ptr<std::set<base::FilePath>> module_paths,
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  PathSanitizer path_sanitizer;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());
  SCOPED_UMA_HISTOGRAM_TIMER("SBIRS.SuspiciousModuleReportingTime");

  for (const auto& module_path : *module_paths) {
    // TODO(proberge): Skip over modules that have already been reported.

    std::unique_ptr<ClientIncidentReport_IncidentData_SuspiciousModuleIncident>
        suspicious_module(
            new ClientIncidentReport_IncidentData_SuspiciousModuleIncident());

    // Sanitized path.
    base::FilePath sanitized_path(module_path);
    path_sanitizer.StripHomeDirectory(&sanitized_path);
    suspicious_module->set_path(sanitized_path.AsUTF8Unsafe());

    // Digest.
    binary_feature_extractor->ExtractDigest(
        module_path, suspicious_module->mutable_digest());

    // Version.
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(module_path));
    if (version_info) {
      base::string16 file_version = version_info->file_version();
      if (!file_version.empty())
        suspicious_module->set_version(base::UTF16ToUTF8(file_version));
    }

    // Signature.
    binary_feature_extractor->CheckSignature(
        module_path, suspicious_module->mutable_signature());

    // Image headers.
    if (!binary_feature_extractor->ExtractImageFeatures(
            module_path, BinaryFeatureExtractor::kDefaultOptions,
            suspicious_module->mutable_image_headers(),
            nullptr /* signed_data */)) {
      suspicious_module->clear_image_headers();
    }

    // Send the incident to the reporting service.
    incident_receiver->AddIncidentForProcess(
        base::MakeUnique<SuspiciousModuleIncident>(
            std::move(suspicious_module)));
  }
}

void CheckModuleWhitelistOnIOThread(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    std::unique_ptr<IncidentReceiver> incident_receiver,
    std::unique_ptr<std::set<ModuleInfo>> module_info_set) {
  SCOPED_UMA_HISTOGRAM_TIMER("SBIRS.SuspiciousModuleDetectionTime");
  std::unique_ptr<std::set<base::FilePath>> suspicious_paths(
      new std::set<base::FilePath>);

  base::FilePath file_path;
  for (const ModuleInfo& module_info : *module_info_set) {
    file_path = base::FilePath(module_info.name);
    base::string16 module_file_name(
        base::i18n::FoldCase(file_path.BaseName().AsUTF16Unsafe()));

    // If not whitelisted.
    if (!database_manager->MatchModuleWhitelistString(
            base::UTF16ToUTF8(module_file_name)))
      suspicious_paths->insert(file_path);
  }

  UMA_HISTOGRAM_COUNTS("SBIRS.SuspiciousModuleReportCount",
                       suspicious_paths->size());

  if (!suspicious_paths->empty()) {
    content::BrowserThread::GetBlockingPool()
        ->PostWorkerTaskWithShutdownBehavior(
            FROM_HERE, base::Bind(&ReportIncidentsForSuspiciousModules,
                                  base::Passed(std::move(suspicious_paths)),
                                  base::Passed(std::move(incident_receiver))),
            base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  }
}

}  // namespace

void VerifyModuleLoadState(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  std::unique_ptr<std::set<ModuleInfo>> module_info_set(
      new std::set<ModuleInfo>);
  if (!GetLoadedModules(module_info_set.get()))
    return;

  // PostTaskAndReply doesn't work here because we're in a sequenced blocking
  // thread pool.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CheckModuleWhitelistOnIOThread, database_manager,
                 base::Passed(std::move(incident_receiver)),
                 base::Passed(std::move(module_info_set))));
}

}  // namespace safe_browsing
