// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"

#include <string.h>

#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/default_component_installer.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using content::BrowserThread;
using content::PluginService;

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

namespace {

// CRX hash. The extension id is: oimompecagnajdejgnnjijobebaeigek.
const uint8 kSha2Hash[] = { 0xe8, 0xce, 0xcf, 0x42, 0x06, 0xd0, 0x93, 0x49,
                            0x6d, 0xd9, 0x89, 0xe1, 0x41, 0x04, 0x86, 0x4a,
                            0x8f, 0xbd, 0x86, 0x12, 0xb9, 0x58, 0x9b, 0xfb,
                            0x4f, 0xbb, 0x1b, 0xa9, 0xd3, 0x85, 0x37, 0xef };

// File name of the Widevine CDM component manifest on different platforms.
const char kWidevineCdmManifestName[] = "WidevineCdm";

// Name of the Widevine CDM OS in the component manifest.
const char kWidevineCdmPlatform[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#else  // OS_LINUX, etc. TODO(viettrungluu): Separate out Chrome OS and Android?
    "linux";
#endif

// Name of the Widevine CDM architecture in the component manifest.
const char kWidevineCdmArch[] =
#if defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// Widevine CDM is packaged as a multi-CRX. Widevine CDM binaries are located in
// _platform_specific/<platform_arch> folder in the package. This function
// returns the platform-specific subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDirectory(const base::FilePath& base_path) {
  std::string platform_arch = kWidevineCdmPlatform;
  platform_arch += '_';
  platform_arch += kWidevineCdmArch;
  return base_path.AppendASCII("_platform_specific").AppendASCII(platform_arch);
}

bool MakeWidevineCdmPluginInfo(
    const base::Version& version,
    const base::FilePath& path,
    const std::vector<base::string16>& additional_param_names,
    const std::vector<base::string16>& additional_param_values,
    content::PepperPluginInfo* plugin_info) {
  if (!version.IsValid() ||
      version.components().size() !=
          static_cast<size_t>(kWidevineCdmVersionNumComponents)) {
    return false;
  }

  plugin_info->is_internal = false;
  // Widevine CDM must run out of process.
  plugin_info->is_out_of_process = true;
  plugin_info->path = path;
  plugin_info->name = kWidevineCdmDisplayName;
  plugin_info->description = kWidevineCdmDescription;
  plugin_info->version = version.GetString();
  content::WebPluginMimeType widevine_cdm_mime_type(
      kWidevineCdmPluginMimeType,
      kWidevineCdmPluginExtension,
      kWidevineCdmPluginMimeTypeDescription);
  widevine_cdm_mime_type.additional_param_names = additional_param_names;
  widevine_cdm_mime_type.additional_param_values = additional_param_values;
  plugin_info->mime_types.push_back(widevine_cdm_mime_type);
  plugin_info->permissions = kWidevineCdmPluginPermissions;

  return true;
}

void GetAdditionalParams(const base::DictionaryValue& manifest,
                         std::vector<base::string16>* additional_param_names,
                         std::vector<base::string16>* additional_param_values) {
  base::string16 codecs;
  if (manifest.GetString("x-cdm-codecs", &codecs)) {
    DLOG_IF(WARNING, codecs.empty())
        << "Widevine CDM component manifest has empty codecs list";
    additional_param_names->push_back(
        base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
    additional_param_values->push_back(codecs);
  } else {
    DLOG(WARNING) << "Widevine CDM component manifest is missing codecs";
    // TODO(ddorwin): Remove this once all users have been updated.
    // The original manifests did not include this string, so add the base set.
    additional_param_names->push_back(
        base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
    additional_param_values->push_back(base::ASCIIToUTF16("vp8,vorbis"));
  }
}

void RegisterWidevineCdmWithChrome(const base::Version& version,
                                   const base::FilePath& path,
                                   scoped_ptr<base::DictionaryValue> manifest) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  GetAdditionalParams(
      *manifest, &additional_param_names, &additional_param_values);
  content::PepperPluginInfo plugin_info;
  if (!MakeWidevineCdmPluginInfo(version,
                                 path,
                                 additional_param_names,
                                 additional_param_values,
                                 &plugin_info)) {
    return;
  }

  // true = Add to beginning of list to override any existing registrations.
  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  PluginService::GetInstance()->RefreshPlugins();
}

}  // namespace

class WidevineCdmComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  WidevineCdmComponentInstallerTraits();
  virtual ~WidevineCdmComponentInstallerTraits() {}

 private:
  // The following methods override ComponentInstallerTraits.
  virtual bool CanAutoUpdate() const OVERRIDE;
  virtual bool OnCustomInstall(const base::DictionaryValue& manifest,
                               const base::FilePath& install_dir) OVERRIDE;
  virtual bool VerifyInstallation(
      const base::FilePath& install_dir) const OVERRIDE;
  virtual void ComponentReady(
      const base::Version& version,
      const base::FilePath& path,
      scoped_ptr<base::DictionaryValue> manifest) OVERRIDE;
  virtual base::FilePath GetBaseDirectory() const OVERRIDE;
  virtual void GetHash(std::vector<uint8>* hash) const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  DISALLOW_COPY_AND_ASSIGN(WidevineCdmComponentInstallerTraits);
};

WidevineCdmComponentInstallerTraits::WidevineCdmComponentInstallerTraits() {
}

bool WidevineCdmComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

// The adapter is copied into the install directory as part of the installation.
bool WidevineCdmComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_path) {
  base::FilePath adapter_install_path = GetPlatformDirectory(install_path)
      .AppendASCII(kWidevineCdmAdapterFileName);
  base::FilePath adapter_source_path;
  PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
  return base::CopyFile(adapter_source_path, adapter_install_path);
}

// Once the component is installed, register the new version with Chrome.
void WidevineCdmComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    scoped_ptr<base::DictionaryValue> manifest) {
  // TODO(ddorwin): Check API version compatibility. Return if fails.
  base::FilePath adapter_install_path = GetPlatformDirectory(path)
      .AppendASCII(kWidevineCdmAdapterFileName);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &RegisterWidevineCdmWithChrome,
      version, adapter_install_path, base::Passed(&manifest)));
}

bool WidevineCdmComponentInstallerTraits::VerifyInstallation(
    const base::FilePath& install_dir) const {
  return base::PathExists(GetPlatformDirectory(install_dir)
          .AppendASCII(kWidevineCdmFileName))
      && base::PathExists(GetPlatformDirectory(install_dir)
          .AppendASCII(kWidevineCdmAdapterFileName));
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\.
base::FilePath WidevineCdmComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(chrome::DIR_COMPONENT_WIDEVINE_CDM, &result);
  return result;
}

void WidevineCdmComponentInstallerTraits::GetHash(
    std::vector<uint8>* hash) const {
  hash->assign(kSha2Hash, kSha2Hash + arraysize(kSha2Hash));
}

std::string WidevineCdmComponentInstallerTraits::GetName() const {
  return kWidevineCdmManifestName;
}

#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

void RegisterWidevineCdmComponent(ComponentUpdateService* cus) {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  base::FilePath adapter_source_path;
  PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
  if (!base::PathExists(adapter_source_path))
    return;
  scoped_ptr<ComponentInstallerTraits> traits(
      new WidevineCdmComponentInstallerTraits);
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer
      = new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus);
#else
  return;
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
}
