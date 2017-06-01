// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_win.h"

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
  if (!IsThirdPartyModule(module_data))
    return;

  std::vector<base::string16> program_names;
  bool uninstallable = installed_programs_.GetInstalledProgramNames(
      module_key.module_path, &program_names);
  UMA_HISTOGRAM_BOOLEAN("ThirdPartyModules.Uninstallable", uninstallable);
}

bool ThirdPartyMetricsRecorder::IsThirdPartyModule(
    const ModuleInfoData& module_data) {
  static const wchar_t kMicrosoft[] = L"Microsoft ";
  static const wchar_t kGoogle[] = L"Google Inc";

  const base::string16& certificate_subject =
      module_data.inspection_result->certificate_info.subject;

  // Check if the signer name begins with "Microsoft ". Signatures are
  // typically "Microsoft Corporation" or "Microsoft Windows", but others
  // may exist.
  if (base::StartsWith(certificate_subject, kMicrosoft,
                       base::CompareCase::SENSITIVE))
    return false;

  return certificate_subject != kGoogle;
}

void ThirdPartyMetricsRecorder::OnInstalledProgramsInitialized(
    ModuleDatabase* module_database) {
  module_database->AddObserver(this);
}
