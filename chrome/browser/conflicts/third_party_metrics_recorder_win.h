// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_

#include "base/macros.h"
#include "chrome/browser/conflicts/installed_programs_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

class ModuleDatabase;
struct ModuleInfoData;
struct ModuleInfoKey;

// Records metrics about third party modules loaded into Chrome.
class ThirdPartyMetricsRecorder : public ModuleDatabaseObserver {
 public:
  explicit ThirdPartyMetricsRecorder(ModuleDatabase* module_database);
  ~ThirdPartyMetricsRecorder() override;

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;

 private:
  void OnInstalledProgramsInitialized(ModuleDatabase* module_database);

  // Returns true if |module_data| is a third party module. Third party modules
  // are defined as not being signed by Google or Microsoft.
  bool IsThirdPartyModule(const ModuleInfoData& module_data);

  InstalledPrograms installed_programs_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsRecorder);
};

#endif  // CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_
