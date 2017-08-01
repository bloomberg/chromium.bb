// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_win.h"

namespace {

// Returns true if the module is signed by Google.
bool IsGoogleModule(const ModuleInfoData& module_data) {
  static const wchar_t kGoogle[] = L"Google Inc";
  return module_data.inspection_result->certificate_info.subject == kGoogle;
}

// Returns true if the signer name begins with "Microsoft ". Signatures are
// typically "Microsoft Corporation" or "Microsoft Windows", but others may
// exist.
bool IsMicrosoftModule(const ModuleInfoData& module_data) {
  static const wchar_t kMicrosoft[] = L"Microsoft ";
  return base::StartsWith(
      module_data.inspection_result->certificate_info.subject, kMicrosoft,
      base::CompareCase::SENSITIVE);
}

// Returns true if |module_data| is a third party module.
bool IsThirdPartyModule(const ModuleInfoData& module_data) {
  return !IsGoogleModule(module_data) && !IsMicrosoftModule(module_data);
}

}  // namespace

ThirdPartyMetricsRecorder::ThirdPartyMetricsRecorder(
    ModuleDatabase* module_database) {
  // base::Unretained() is safe here because ThirdPartyMetricsRecorder owns
  // |installed_programs_| and the callback won't be invoked if this instance is
  // destroyed.
  installed_programs_.Initialize(
      base::Bind(&ThirdPartyMetricsRecorder::OnInstalledProgramsInitialized,
                 base::Unretained(this), module_database));
}

ThirdPartyMetricsRecorder::~ThirdPartyMetricsRecorder() = default;

void ThirdPartyMetricsRecorder::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  const CertificateInfo& certificate_info =
      module_data.inspection_result->certificate_info;
  module_count_++;
  if (certificate_info.type != CertificateType::NO_CERTIFICATE) {
    ++signed_module_count_;

    if (certificate_info.type == CertificateType::CERTIFICATE_IN_CATALOG)
      ++catalog_module_count_;

    if (IsMicrosoftModule(module_data)) {
      ++microsoft_module_count_;
    } else if (IsGoogleModule(module_data)) {
      // No need to count these explicitly.
    } else {
      // Count modules that are neither signed by Google nor Microsoft.
      // These are considered "third party" modules.
      if (module_data.module_types & ModuleInfoData::kTypeLoadedModule) {
        ++loaded_third_party_module_count_;
      } else {
        ++not_loaded_third_party_module_count_;
      }
    }
  }

  // The uninstallable metric is only recorded for third party metrics.
  if (IsThirdPartyModule(module_data)) {
    std::vector<base::string16> program_names;
    bool uninstallable = installed_programs_.GetInstalledProgramNames(
        module_key.module_path, &program_names);
    UMA_HISTOGRAM_BOOLEAN("ThirdPartyModules.Uninstallable", uninstallable);
  }
}

void ThirdPartyMetricsRecorder::OnModuleDatabaseIdle() {
  if (metrics_emitted_)
    return;
  metrics_emitted_ = true;

  // Report back some metrics regarding third party modules and certificates.
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Loaded",
                                 loaded_third_party_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.NotLoaded",
                                 not_loaded_third_party_module_count_, 1, 500,
                                 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed",
                                 signed_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed.Microsoft",
                                 microsoft_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed.Catalog",
                                 catalog_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Total",
                                 module_count_, 1, 500, 50);
}

void ThirdPartyMetricsRecorder::OnInstalledProgramsInitialized(
    ModuleDatabase* module_database) {
  module_database->AddObserver(this);
}
