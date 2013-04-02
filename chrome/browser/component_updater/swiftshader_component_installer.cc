// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/swiftshader_component_installer.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/gpu_feature_type.h"

using content::BrowserThread;
using content::GpuDataManager;

namespace {

// CRX hash. The extension id is: nhfgdggnnopgbfdlpeoalgcjdgfafocg.
const uint8 sha2_hash[] = {0xd7, 0x56, 0x36, 0x6d, 0xde, 0xf6, 0x15, 0x3b,
                           0xf4, 0xe0, 0xb6, 0x29, 0x36, 0x50, 0x5e, 0x26,
                           0xbd, 0x77, 0x8b, 0x8e, 0x35, 0xc2, 0x7e, 0x43,
                           0x52, 0x47, 0x62, 0xed, 0x12, 0xca, 0xcc, 0x6a};

// File name of the internal SwiftShader plugin on different platforms.
const base::FilePath::CharType kSwiftShaderEglName[] =
    FILE_PATH_LITERAL("libegl.dll");
const base::FilePath::CharType kSwiftShaderGlesName[] =
    FILE_PATH_LITERAL("libglesv2.dll");

const char kSwiftShaderManifestName[] = "SwiftShader";

const base::FilePath::CharType kSwiftShaderBaseDirectory[] =
    FILE_PATH_LITERAL("SwiftShader");

// If we don't have a SwiftShader component, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// The base directory on windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\SwiftShader\.
base::FilePath GetSwiftShaderBaseDirectory() {
  base::FilePath result;
  PathService::Get(chrome::DIR_USER_DATA, &result);
  return result.Append(kSwiftShaderBaseDirectory);
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
  file_util::FileEnumerator
      file_enumerator(base_dir, false, file_util::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (version.CompareTo(*latest) > 0 &&
        file_util::PathExists(path.Append(kSwiftShaderEglName)) &&
        file_util::PathExists(path.Append(kSwiftShaderGlesName))) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GpuDataManager::GetInstance()->RegisterSwiftShaderPath(path);
}

}  // namespace

class SwiftShaderComponentInstaller : public ComponentInstaller {
 public:
  explicit SwiftShaderComponentInstaller(const Version& version);

  virtual ~SwiftShaderComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(base::DictionaryValue* manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

 private:
  Version current_version_;
};

SwiftShaderComponentInstaller::SwiftShaderComponentInstaller(
    const Version& version) : current_version_(version) {
  DCHECK(version.IsValid());
}

void SwiftShaderComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "SwiftShader update error: " << error;
}

bool SwiftShaderComponentInstaller::Install(base::DictionaryValue* manifest,
                                            const base::FilePath& unpack_path) {
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kSwiftShaderManifestName)
    return false;
  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) >= 0)
    return false;
  if (!file_util::PathExists(unpack_path.Append(kSwiftShaderEglName)) ||
      !file_util::PathExists(unpack_path.Append(kSwiftShaderGlesName)))
    return false;
  // Passed the basic tests. Time to install it.
  base::FilePath path =
      GetSwiftShaderBaseDirectory().AppendASCII(version.GetString());
  if (file_util::PathExists(path))
    return false;
  if (!file_util::Move(unpack_path, path))
    return false;
  // Installation is done. Now tell the rest of chrome.
  current_version_ = version;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&RegisterSwiftShaderWithChrome, path));
  return true;
}

void FinishSwiftShaderUpdateRegistration(ComponentUpdateService* cus,
                                         const Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CrxComponent swiftshader;
  swiftshader.name = "Swift Shader";
  swiftshader.installer = new SwiftShaderComponentInstaller(version);
  swiftshader.version = version;
  swiftshader.pk_hash.assign(sha2_hash, &sha2_hash[sizeof(sha2_hash)]);
  if (cus->RegisterComponent(swiftshader) != ComponentUpdateService::kOk) {
    NOTREACHED() << "SwiftShader component registration fail";
  }
}

class UpdateChecker : public content::GpuDataManagerObserver {
 public:
  explicit UpdateChecker(ComponentUpdateService* cus);

  virtual void OnGpuInfoUpdate() OVERRIDE;

 private:
  ComponentUpdateService* cus_;
};

UpdateChecker::UpdateChecker(ComponentUpdateService* cus)
  : cus_(cus) {
}

void UpdateChecker::OnGpuInfoUpdate() {
  GpuDataManager *gpu_data_manager = GpuDataManager::GetInstance();

  if (!gpu_data_manager->GpuAccessAllowed() ||
      gpu_data_manager->IsFeatureBlacklisted(content::GPU_FEATURE_TYPE_WEBGL) ||
      gpu_data_manager->ShouldUseSoftwareRendering()) {
    gpu_data_manager->RemoveObserver(this);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    base::FilePath path = GetSwiftShaderBaseDirectory();

    Version version(kNullVersion);
    GetLatestSwiftShaderDirectory(&path, &version, NULL);

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&FinishSwiftShaderUpdateRegistration, cus_, version));
  }
}

// Check if there already is a version of swiftshader installed,
// and if so register it.
void RegisterSwiftShaderPath(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = GetSwiftShaderBaseDirectory();
  if (!file_util::PathExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      NOTREACHED() << "Could not create SwiftShader directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetLatestSwiftShaderDirectory(&path, &version, &older_dirs))
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&RegisterSwiftShaderWithChrome, path));

  UpdateChecker *update_checker = new UpdateChecker(cus);
  GpuDataManager::GetInstance()->AddObserver(update_checker);
  update_checker->OnGpuInfoUpdate();
  // We leak update_checker here, because it has to stick around for the life
  // of the GpuDataManager.

  // Remove older versions of SwiftShader.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    file_util::Delete(*iter, true);
  }
}

void RegisterSwiftShaderComponent(ComponentUpdateService* cus) {
#if defined(ENABLE_SWIFTSHADER)
  base::CPU cpu;

  if (!cpu.has_sse2())
    return;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&RegisterSwiftShaderPath, cus));
#endif
}
