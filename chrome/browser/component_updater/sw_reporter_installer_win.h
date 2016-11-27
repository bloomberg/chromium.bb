// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "components/component_updater/default_component_installer.h"

class PrefRegistrySimple;

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace component_updater {

class ComponentUpdateService;

// These MUST match the values for SwReporterExperimentError in histograms.xml.
// Exposed for testing.
enum SwReporterExperimentError {
  SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG = 0,
  SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS = 1,
  SW_REPORTER_EXPERIMENT_ERROR_MAX,
};

// Callback for running the software reporter after it is downloaded.
using SwReporterRunner =
    base::Callback<void(const safe_browsing::SwReporterQueue& invocations,
                        const base::Version& version)>;

class SwReporterInstallerTraits : public ComponentInstallerTraits {
 public:
  SwReporterInstallerTraits(const SwReporterRunner& reporter_runner,
                            bool is_experimental_engine_supported);
  ~SwReporterInstallerTraits() override;

  // ComponentInstallerTraits implementation.
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

 private:
  friend class SwReporterInstallerTest;

  // Returns true if the experimental engine is supported and the Feature is
  // enabled.
  bool IsExperimentalEngineEnabled() const;

  SwReporterRunner reporter_runner_;
  const bool is_experimental_engine_supported_;

  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerTraits);
};

// Call once during startup to make the component update service aware of the
// SwReporter.
void RegisterSwReporterComponent(ComponentUpdateService* cus);

// Register local state preferences related to the SwReporter.
void RegisterPrefsForSwReporter(PrefRegistrySimple* registry);

// Register profile preferences related to the SwReporter.
void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
