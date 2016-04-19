// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/caps_installer_win.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"

namespace component_updater {

namespace {

// The values of the enum cannot be changed and should be mirrored with
// the values of CAPSUpdaterStep in metrics/histograms/histograms.xml. Only
// add new values at the end.
enum CAPSUmaValue {
  CAPS_COMPONENT_READY = 0,
  CAPS_COMPONENT_MISSING = 1,
  CAPS_SERVICE_FAILED_TO_START = 2,
  CAPS_SERVICE_STARTED = 3,
  CAPS_UMA_MAX
};

void ReportUmaStep(CAPSUmaValue value) {
  UMA_HISTOGRAM_ENUMERATION("CAPSUpdater.Step", value, CAPS_UMA_MAX);
}

// CRX hash. The extension id is: bcpgokokgekmnfkohklccmonnakdimfh.
const uint8_t kSha256Hash[] = {0x12, 0xf6, 0xea, 0xea, 0x64, 0xac, 0xd5, 0xae,
                               0x7a, 0xb2, 0x2c, 0xed, 0xd0, 0xa3, 0x8c, 0x57,
                               0x49, 0x05, 0x8f, 0x7d, 0x14, 0xa4, 0x22, 0x4d,
                               0x9b, 0xf6, 0x14, 0x99, 0xdf, 0xf8, 0xc9, 0xb3};

const base::FilePath::CharType kCapsBinary[] =
    FILE_PATH_LITERAL("chrome_crash_svc.exe");

const base::FilePath::CharType kCapsDirectory[] =
    FILE_PATH_LITERAL("Caps");

// This function is called from a worker thread to launch crash service.
void LaunchService(const base::FilePath& exe_path) {
  base::CommandLine service_cmdline(exe_path);
  service_cmdline.AppendSwitch("caps-update");
  base::Process service =
      base::LaunchProcess(service_cmdline, base::LaunchOptions());
  CAPSUmaValue uma_step = service.IsValid() ?
      CAPS_SERVICE_STARTED : CAPS_SERVICE_FAILED_TO_START;
  ReportUmaStep(uma_step);
}

class CAPSInstallerTraits : public ComponentInstallerTraits {
 public:
  CAPSInstallerTraits() {}
  ~CAPSInstallerTraits() override {}

  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override {
    bool has_binary = base::PathExists(dir.Append(kCapsBinary));
    ReportUmaStep(has_binary ? CAPS_COMPONENT_READY : CAPS_COMPONENT_MISSING);
    return has_binary;
  }

  bool CanAutoUpdate() const override { return true; }

  bool RequiresNetworkEncryption() const override { return false; }

  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override {
    return true;
  }

  void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) override {
    // Can't block here. This is usually the browser UI thread.
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&LaunchService, install_dir.Append(kCapsBinary)),
        false);
  }

  // Directory is usually "%appdata%\Local\Chrome\User Data\Caps".
  base::FilePath GetBaseDirectory() const override {
    base::FilePath user_data;
    PathService::Get(chrome::DIR_USER_DATA, &user_data);
    return user_data.Append(kCapsDirectory);
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(kSha256Hash,
                 kSha256Hash + arraysize(kSha256Hash));
  }

  // This string is shown in chrome://components.
  std::string GetName() const override { return "Chrome Crash Service"; }

  std::string GetAp() const override { return std::string(); }
};

}  // namespace

void RegisterCAPSComponent(ComponentUpdateService* cus) {
  // The component updater takes ownership of |installer|.
  std::unique_ptr<ComponentInstallerTraits> traits(new CAPSInstallerTraits());
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
