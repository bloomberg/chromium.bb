// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/swiftshader_component_installer.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "gpu/config/gpu_feature_type.h"

using content::BrowserThread;
using content::GpuDataManager;

namespace component_updater {

namespace {

// CRX hash. The extension id is: nhfgdggnnopgbfdlpeoalgcjdgfafocg.
const uint8_t kSha2Hash[] = {0xd7, 0x56, 0x36, 0x6d, 0xde, 0xf6, 0x15, 0x3b,
                             0xf4, 0xe0, 0xb6, 0x29, 0x36, 0x50, 0x5e, 0x26,
                             0xbd, 0x77, 0x8b, 0x8e, 0x35, 0xc2, 0x7e, 0x43,
                             0x52, 0x47, 0x62, 0xed, 0x12, 0xca, 0xcc, 0x6a};

// File name of the internal SwiftShader plugin on different platforms.
const base::FilePath::CharType kSwiftShaderEglName[] =
    FILE_PATH_LITERAL("libegl.dll");
const base::FilePath::CharType kSwiftShaderGlesName[] =
    FILE_PATH_LITERAL("libglesv2.dll");

const char kSwiftShaderManifestName[] = "SwiftShader";

// If we don't have a SwiftShader component, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// The base directory on windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\SwiftShader\.
base::FilePath GetSwiftShaderBaseDirectory() {
  base::FilePath result;
  if (!PathService::Get(DIR_SWIFT_SHADER, &result))
    NOTREACHED() << "Couldn't get SwiftShader directory.";
  return result;
}

// SwiftShader has version encoded in the path itself
// so we need to enumerate the directories to find the full path.
// On success it returns something like:
// <profile>\AppData\Local\Google\Chrome\User Data\SwiftShader\10.3.44.555\.
bool GetLatestSwiftShaderDirectory(base::FilePath* result,
                                   Version* latest,
                                   std::vector<base::FilePath>* older_dirs) {
  base::FilePath base_dir = GetSwiftShaderBaseDirectory();
  bool found = false;
  base::FileEnumerator file_enumerator(
      base_dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (version.CompareTo(*latest) > 0 &&
        base::PathExists(path.Append(kSwiftShaderEglName)) &&
        base::PathExists(path.Append(kSwiftShaderGlesName))) {
      if (found && older_dirs)
        older_dirs->push_back(*result);
      *latest = version;
      *result = path;
      found = true;
    } else {
      if (older_dirs)
        older_dirs->push_back(path);
    }
  }
  return found;
}

void RegisterSwiftShaderWithChrome(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GpuDataManager::GetInstance()->RegisterSwiftShaderPath(path);
}

class SwiftShaderComponentInstaller : public update_client::CrxInstaller {
 public:
  explicit SwiftShaderComponentInstaller(const Version& version);

  // ComponentInstaller implementation:
  void OnUpdateError(int error) override;

  bool Install(const base::DictionaryValue& manifest,
               const base::FilePath& unpack_path) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

  bool Uninstall() override;

 private:
  ~SwiftShaderComponentInstaller() override {}

  Version current_version_;
};

SwiftShaderComponentInstaller::SwiftShaderComponentInstaller(
    const Version& version)
    : current_version_(version) {
  DCHECK(version.IsValid());
}

void SwiftShaderComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "SwiftShader update error: " << error;
}

bool SwiftShaderComponentInstaller::Install(
    const base::DictionaryValue& manifest,
    const base::FilePath& unpack_path) {
  std::string name;
  manifest.GetStringASCII("name", &name);
  if (name != kSwiftShaderManifestName)
    return false;
  std::string proposed_version;
  manifest.GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) >= 0)
    return false;
  if (!base::PathExists(unpack_path.Append(kSwiftShaderEglName)) ||
      !base::PathExists(unpack_path.Append(kSwiftShaderGlesName)))
    return false;
  // Passed the basic tests. Time to install it.
  base::FilePath path =
      GetSwiftShaderBaseDirectory().AppendASCII(version.GetString());
  if (base::PathExists(path))
    return false;
  if (!base::Move(unpack_path, path))
    return false;
  // Installation is done. Now tell the rest of chrome.
  current_version_ = version;
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&RegisterSwiftShaderWithChrome, path));
  return true;
}

bool SwiftShaderComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  return false;
}

bool SwiftShaderComponentInstaller::Uninstall() {
  return false;
}

void FinishSwiftShaderUpdateRegistration(ComponentUpdateService* cus,
                                         const Version& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  update_client::CrxComponent swiftshader;
  swiftshader.name = "Swift Shader";
  swiftshader.installer = new SwiftShaderComponentInstaller(version);
  swiftshader.version = version;
  swiftshader.pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);
  if (!cus->RegisterComponent(swiftshader)) {
    NOTREACHED() << "SwiftShader component registration fail";
  }
}

class UpdateChecker : public content::GpuDataManagerObserver {
 public:
  explicit UpdateChecker(ComponentUpdateService* cus);

  void OnGpuInfoUpdate() override;

 private:
  ComponentUpdateService* cus_;
};

UpdateChecker::UpdateChecker(ComponentUpdateService* cus) : cus_(cus) {
}

void UpdateChecker::OnGpuInfoUpdate() {
  GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();

  if (!gpu_data_manager->GpuAccessAllowed(NULL) ||
      gpu_data_manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL) ||
      gpu_data_manager->ShouldUseSwiftShader()) {
    gpu_data_manager->RemoveObserver(this);
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    base::FilePath path = GetSwiftShaderBaseDirectory();

    Version version(kNullVersion);
    GetLatestSwiftShaderDirectory(&path, &version, NULL);

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FinishSwiftShaderUpdateRegistration, cus_, version));
  }
}

#if defined(ENABLE_SWIFTSHADER)

// Check if there already is a version of swiftshader installed,
// and if so register it.
void RegisterSwiftShaderPath(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath path = GetSwiftShaderBaseDirectory();
  if (!base::PathExists(path)) {
    if (!base::CreateDirectory(path)) {
      NOTREACHED() << "Could not create SwiftShader directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetLatestSwiftShaderDirectory(&path, &version, &older_dirs))
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&RegisterSwiftShaderWithChrome, path));

  UpdateChecker* update_checker = new UpdateChecker(cus);
  GpuDataManager::GetInstance()->AddObserver(update_checker);
  update_checker->OnGpuInfoUpdate();
  // We leak update_checker here, because it has to stick around for the life
  // of the GpuDataManager.

  // Remove older versions of SwiftShader.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end();
       ++iter) {
    base::DeleteFile(*iter, true);
  }
}

#endif  // ENABLE_SWIFTSHADER

}  // namespace

void RegisterSwiftShaderComponent(ComponentUpdateService* cus) {
#if defined(ENABLE_SWIFTSHADER)
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&RegisterSwiftShaderPath, cus));
#endif
}

}  // namespace component_updater
