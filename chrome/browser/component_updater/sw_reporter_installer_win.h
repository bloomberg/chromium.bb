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
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "components/component_updater/component_installer.h"

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

// Expose the feature name so it can be referenced in tests.
// TODO(crbug.com/786964): This feature will continue to exist as part of a
// permanent variations study to control which version of the reporter gets
// downloaded. Rename it to something that makes sense long-term.
constexpr char kComponentTagFeatureName[] = "ExperimentalSwReporterEngine";

constexpr char kSwReporterComponentId[] = "gkmgaooipdjhmangpemjhigmamcehddo";

// These MUST match the values for SoftwareReporterExperimentError in
// histograms.xml. Exposed for testing.
enum SoftwareReporterExperimentError {
  SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG = 0,
  SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS = 1,
  SW_REPORTER_EXPERIMENT_ERROR_MISSING_PARAMS = 2,
  SW_REPORTER_EXPERIMENT_ERROR_MAX,
};

// Callback for running the software reporter after it is downloaded.
using SwReporterRunner = base::Callback<void(
    safe_browsing::SwReporterInvocationType invocation_type,
    safe_browsing::SwReporterInvocationSequence&& invocations)>;

class SwReporterInstallerPolicy : public ComponentInstallerPolicy {
 public:
  SwReporterInstallerPolicy(
      const SwReporterRunner& reporter_runner,
      safe_browsing::SwReporterInvocationType invocation_type);
  ~SwReporterInstallerPolicy() override;

  // ComponentInstallerPolicy implementation.
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
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

  SwReporterRunner reporter_runner_;

  const safe_browsing::SwReporterInvocationType invocation_type_;

  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerPolicy);
};

// Installs the SwReporter component and runs the reporter once it's available.
// Once ready, this may trigger either a periodic or a user-initiated run of
// the reporter, depending on |invocation_type|.
void RegisterSwReporterComponentWithParams(
    safe_browsing::SwReporterInvocationType invocation_type,
    ComponentUpdateService* cus);

// Call once during startup to make the component update service aware of the
// SwReporter. Once ready, this may trigger a periodic run of the reporter.
void RegisterSwReporterComponent(ComponentUpdateService* cus);

// Register local state preferences related to the SwReporter.
void RegisterPrefsForSwReporter(PrefRegistrySimple* registry);

// Register profile preferences related to the SwReporter.
void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
