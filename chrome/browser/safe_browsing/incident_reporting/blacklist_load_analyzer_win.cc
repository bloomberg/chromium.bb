// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_analyzer.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/safe_browsing/binary_feature_extractor.h"
#include "chrome/browser/safe_browsing/incident_reporting/add_incident_callback.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
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
    base::string16 module_file_name(base::StringToLowerASCII(
        base::FilePath(module_iter->name).BaseName().value()));
    if (blacklist::GetBlacklistIndex(module_file_name.c_str()) != -1) {
      module_names->push_back(module_iter->name);
    }
  }

  return true;
}

void VerifyBlacklistLoadState(const AddIncidentCallback& callback) {
  std::vector<base::string16> module_names;
  if (GetLoadedBlacklistedModules(&module_names)) {
    PathSanitizer path_sanitizer;

    const bool blacklist_intialized = blacklist::IsBlacklistInitialized();

    std::vector<base::string16>::const_iterator module_iter(
        module_names.begin());
    for (; module_iter != module_names.end(); ++module_iter) {
      scoped_ptr<ClientIncidentReport_IncidentData> incident_data(
          new ClientIncidentReport_IncidentData());
      ClientIncidentReport_IncidentData_BlacklistLoadIncident* blacklist_load =
          incident_data->mutable_blacklist_load();

      base::FilePath module_path(*module_iter);
      path_sanitizer.StripHomeDirectory(&module_path);

      blacklist_load->set_path(base::WideToUTF8(module_path.value()));
      // TODO(robertshield): Add computation of file digest and version here.

      blacklist_load->set_blacklist_initialized(blacklist_intialized);

      // Send the report.
      callback.Run(incident_data.Pass());
    }
  }
}

}  // namespace safe_browsing
