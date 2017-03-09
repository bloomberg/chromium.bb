// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_analyzer.h"

#include <utility>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome_elf/blacklist/blacklist.h"

namespace safe_browsing {

// Retrieves the set of blacklisted modules that are loaded in the process.
// Returns true if successful, false otherwise.
bool GetLoadedBlacklistedModules(std::vector<base::string16>* module_names) {
  DCHECK(module_names);

  std::set<ModuleInfo> module_info_set;
  if (!GetLoadedModules(&module_info_set))
    return false;

  std::set<ModuleInfo>::const_iterator module_iter(module_info_set.begin());
  for (; module_iter != module_info_set.end(); ++module_iter) {
    base::string16 module_file_name(base::ToLowerASCII(
        base::FilePath(module_iter->name).BaseName().value()));
    if (blacklist::GetBlacklistIndex(module_file_name.c_str()) != -1) {
      module_names->push_back(module_iter->name);
    }
  }

  return true;
}

void VerifyBlacklistLoadState(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  std::vector<base::string16> module_names;
  if (GetLoadedBlacklistedModules(&module_names)) {
    PathSanitizer path_sanitizer;

    const bool blacklist_intialized = blacklist::IsBlacklistInitialized();

    for (const auto& module_name : module_names) {
      std::unique_ptr<ClientIncidentReport_IncidentData_BlacklistLoadIncident>
          blacklist_load(
              new ClientIncidentReport_IncidentData_BlacklistLoadIncident());

      const base::FilePath module_path(module_name);

      // Sanitized path.
      base::FilePath sanitized_path(module_path);
      path_sanitizer.StripHomeDirectory(&sanitized_path);
      blacklist_load->set_path(base::WideToUTF8(sanitized_path.value()));

      // Digest.
      scoped_refptr<BinaryFeatureExtractor>
          binary_feature_extractor(new BinaryFeatureExtractor());
      base::TimeTicks start_time = base::TimeTicks::Now();
      binary_feature_extractor->ExtractDigest(module_path,
                                              blacklist_load->mutable_digest());
      UMA_HISTOGRAM_TIMES("SBIRS.BLAHashTime",
                          base::TimeTicks::Now() - start_time);

      // Version.
      std::unique_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfo(module_path));
      if (version_info) {
        std::wstring file_version = version_info->file_version();
        if (!file_version.empty())
          blacklist_load->set_version(base::WideToUTF8(file_version));
      }

      // Initialized state.
      blacklist_load->set_blacklist_initialized(blacklist_intialized);

      // Signature.
      start_time = base::TimeTicks::Now();
      binary_feature_extractor->CheckSignature(
          module_path, blacklist_load->mutable_signature());
      UMA_HISTOGRAM_TIMES("SBIRS.BLASignatureTime",
                          base::TimeTicks::Now() - start_time);

      // Image headers.
      if (!binary_feature_extractor->ExtractImageFeatures(
              module_path,
              BinaryFeatureExtractor::kDefaultOptions,
              blacklist_load->mutable_image_headers(),
              nullptr /* signed_data */)) {
        blacklist_load->clear_image_headers();
      }

      // Send the report.
      incident_receiver->AddIncidentForProcess(
          base::MakeUnique<BlacklistLoadIncident>(std::move(blacklist_load)));
    }
  }
}

}  // namespace safe_browsing
