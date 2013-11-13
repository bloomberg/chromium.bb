// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/install_verification.h"

#include <set>
#include <vector>

#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/install_verification/win/imported_module_verification.h"
#include "chrome/browser/install_verification/win/loaded_module_verification.h"
#include "chrome/browser/install_verification/win/module_ids.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"

namespace {

void ReportModuleMatch(size_t module_id) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("InstallVerifier.ModuleMatch", module_id);
}

void ReportImport(size_t module_id) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("InstallVerifier.ModuleImport", module_id);
}

}  // namespace

void VerifyInstallation() {
  ModuleIDs module_ids;
  LoadModuleIDs(&module_ids);
  std::set<ModuleInfo> loaded_modules;
  if (GetLoadedModules(&loaded_modules)) {
    VerifyLoadedModules(loaded_modules, module_ids, &ReportModuleMatch);
    VerifyImportedModules(loaded_modules, module_ids, &ReportImport);
  }
}
