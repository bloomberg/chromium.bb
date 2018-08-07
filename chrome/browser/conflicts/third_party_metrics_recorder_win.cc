// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "components/crash/core/common/crash_key.h"

namespace {

// Returns true if the module is signed by Google.
bool IsGoogleModule(base::StringPiece16 subject) {
  static const wchar_t kGoogle[] = L"Google Inc";
  return subject == kGoogle;
}

}  // namespace

ThirdPartyMetricsRecorder::ThirdPartyMetricsRecorder() {
  current_value_.reserve(kCrashKeySize);
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

    base::StringPiece16 certificate_subject = certificate_info.subject;
    if (IsMicrosoftModule(certificate_subject)) {
      ++microsoft_module_count_;
    } else if (IsGoogleModule(certificate_subject)) {
      // No need to count these explicitly.
    } else {
      // Count modules that are neither signed by Google nor Microsoft.
      // These are considered "third party" modules.
      if (module_data.module_properties &
          ModuleInfoData::kPropertyLoadedModule) {
        ++loaded_third_party_module_count_;
      } else {
        ++not_loaded_third_party_module_count_;
      }
    }
  } else {
    ++unsigned_module_count_;

    // Put unsigned modules into the crash keys.
    if (module_data.module_properties & ModuleInfoData::kPropertyLoadedModule)
      AddUnsignedModuleToCrashkeys(module_data.inspection_result->basename);
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
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Unsigned",
                                 unsigned_module_count_, 1, 500, 50);
}

void ThirdPartyMetricsRecorder::AddUnsignedModuleToCrashkeys(
    const base::string16& module_basename) {
  using UnsignedModulesKey = crash_reporter::CrashKeyString<kCrashKeySize>;
  static UnsignedModulesKey unsigned_modules_keys[] = {
      {"unsigned-modules-1", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-2", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-3", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-4", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-5", UnsignedModulesKey::Tag::kArray},
  };

  if (current_key_index_ >= base::size(unsigned_modules_keys))
    return;

  std::string module = base::UTF16ToUTF8(module_basename);

  // Truncate the basename if it doesn't fit in one crash key.
  size_t module_length = std::min(module.length(), kCrashKeySize);

  // Check if the module fits in the current string or if a new string is
  // needed.
  size_t length_remaining = kCrashKeySize;
  if (!current_value_.empty())
    length_remaining -= current_value_.length() + 1;

  if (module_length > length_remaining) {
    current_value_.clear();

    if (++current_key_index_ >= base::size(unsigned_modules_keys))
      return;
  }

  // Append the module to the current string. Separate with a comma if needed.
  if (!current_value_.empty())
    current_value_.append(",");
  current_value_.append(module, 0, module_length);

  unsigned_modules_keys[current_key_index_].Set(current_value_);
}
