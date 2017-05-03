// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/pepper_plugin_info.h"
#include "media/cdm/supported_cdm_versions.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR. NOLINT

using content::BrowserThread;
using content::CdmRegistry;
using content::PluginService;

namespace component_updater {

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

namespace {

// CRX hash. The extension id is: oimompecagnajdejgnnjijobebaeigek.
const uint8_t kSha2Hash[] = {0xe8, 0xce, 0xcf, 0x42, 0x06, 0xd0, 0x93, 0x49,
                             0x6d, 0xd9, 0x89, 0xe1, 0x41, 0x04, 0x86, 0x4a,
                             0x8f, 0xbd, 0x86, 0x12, 0xb9, 0x58, 0x9b, 0xfb,
                             0x4f, 0xbb, 0x1b, 0xa9, 0xd3, 0x85, 0x37, 0xef};

// File name of the Widevine CDM adapter version file. The CDM adapter shares
// the same version number with Chromium version.
const char kCdmAdapterVersionName[] = "CdmAdapterVersion";

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

// The CDM manifest includes several custom values, all beginning with "x-cdm-".
// All values are strings.
// All values that are lists are delimited by commas. No trailing commas.
// For example, "1,2,4".
const char kCdmValueDelimiter = ',';
static_assert(kCdmValueDelimiter == kCdmSupportedCodecsValueDelimiter,
              "cdm delimiters must match");
// The following entries are required.
//  Interface versions are lists of integers (e.g. "1" or "1,2,4").
//  These are checked in this file before registering the CDM.
//  All match the interface versions from content_decryption_module.h that the
//  CDM supports.
//    Matches CDM_MODULE_VERSION.
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
//    Matches supported ContentDecryptionModule_* version(s).
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
//    Matches supported Host_* version(s).
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
//  The codecs list is a list of simple codec names (e.g. "vp8,vorbis").
//  The list is passed to other parts of Chrome.
const char kCdmCodecsListName[] = "x-cdm-codecs";

// TODO(xhwang): Move this to a common place if needed.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Widevine CDM is packaged as a multi-CRX. Widevine CDM binaries are located in
// _platform_specific/<platform_arch> folder in the package. This function
// returns the platform-specific subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDirectory(const base::FilePath& base_path) {
  std::string platform_arch = kWidevineCdmPlatform;
  platform_arch += '_';
  platform_arch += kWidevineCdmArch;
  return base_path.AppendASCII("_platform_specific").AppendASCII(platform_arch);
}

bool MakeWidevineCdmPluginInfo(const base::Version& version,
                               const base::FilePath& cdm_install_dir,
                               const std::string& codecs,
                               content::PepperPluginInfo* plugin_info) {
  if (!version.IsValid() ||
      version.components().size() !=
          static_cast<size_t>(kWidevineCdmVersionNumComponents)) {
    DVLOG(1) << "Invalid version.";
    return false;
  }

  plugin_info->is_internal = false;
  // Widevine CDM must run out of process.
  plugin_info->is_out_of_process = true;
  plugin_info->path = GetPlatformDirectory(cdm_install_dir)
                          .AppendASCII(kWidevineCdmAdapterFileName);
  plugin_info->name = kWidevineCdmDisplayName;
  plugin_info->description = kWidevineCdmDescription +
                             std::string(" (version: ") + version.GetString() +
                             ")";
  plugin_info->version = version.GetString();
  content::WebPluginMimeType widevine_cdm_mime_type(
      kWidevineCdmPluginMimeType,
      kWidevineCdmPluginExtension,
      kWidevineCdmPluginMimeTypeDescription);
  widevine_cdm_mime_type.additional_param_names.push_back(
      base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
  widevine_cdm_mime_type.additional_param_values.push_back(
      base::ASCIIToUTF16(codecs));

  plugin_info->mime_types.push_back(widevine_cdm_mime_type);
  plugin_info->permissions = kWidevineCdmPluginPermissions;

  return true;
}

typedef bool (*VersionCheckFunc)(int version);

bool CheckForCompatibleVersion(const base::DictionaryValue& manifest,
                               const std::string version_name,
                               VersionCheckFunc version_check_func) {
  std::string versions_string;
  if (!manifest.GetString(version_name, &versions_string)) {
    DVLOG(1) << "Widevine CDM component manifest missing " << version_name;
    return false;
  }
  DVLOG_IF(1, versions_string.empty())
      << "Widevine CDM component manifest has empty " << version_name;

  for (const base::StringPiece& ver_str : base::SplitStringPiece(
           versions_string, std::string(1, kCdmValueDelimiter),
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int version = 0;
    if (base::StringToInt(ver_str, &version))
      if (version_check_func(version))
        return true;
  }

  DVLOG(1) << "Widevine CDM component manifest has no supported "
           << version_name << " in '" << versions_string << "'";
  return false;
}

// Returns whether the CDM's API versions, as specified in the manifest, are
// compatible with this Chrome binary.
// Checks the module API, CDM interface API, and Host API.
// This should never fail except in rare cases where the component has not been
// updated recently or the user downgrades Chrome.
bool IsCompatibleWithChrome(const base::DictionaryValue& manifest) {
  return CheckForCompatibleVersion(manifest,
                                   kCdmModuleVersionsName,
                                   media::IsSupportedCdmModuleVersion) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmInterfaceVersionsName,
                                   media::IsSupportedCdmInterfaceVersion) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmHostVersionsName,
                                   media::IsSupportedCdmHostVersion);
}

std::string GetCodecs(const base::DictionaryValue& manifest) {
  std::string codecs;
  if (manifest.GetStringASCII(kCdmCodecsListName, &codecs)) {
    DVLOG_IF(1, codecs.empty())
        << "Widevine CDM component manifest has empty codecs list";
  } else {
    DVLOG(1) << "Widevine CDM component manifest is missing codecs";
  }
  return codecs;
}

void RegisterWidevineCdmWithChrome(
    const base::Version& cdm_version,
    const base::FilePath& cdm_install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string codecs = GetCodecs(*manifest);

  content::PepperPluginInfo plugin_info;
  if (!MakeWidevineCdmPluginInfo(cdm_version, cdm_install_dir, codecs,
                                 &plugin_info)) {
    return;
  }

  VLOG(1) << "Register Widevine CDM with Chrome";

  // true = Add to beginning of list to override any existing registrations.
  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  // Tell the browser to refresh the plugin list. Then tell all renderers to
  // update their plugin list caches.
  PluginService::GetInstance()->RefreshPlugins();
  PluginService::GetInstance()->PurgePluginListCache(NULL, false);

  // Also register Widevine with the CdmRegistry.
  const base::FilePath cdm_path =
      GetPlatformDirectory(cdm_install_dir)
          .AppendASCII(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  const std::vector<std::string> supported_codecs = base::SplitString(
      codecs, std::string(1, kCdmSupportedCodecsValueDelimiter),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CdmRegistry::GetInstance()->RegisterCdm(content::CdmInfo(
      kWidevineCdmType, cdm_version, cdm_path, supported_codecs));
}

}  // namespace

class WidevineCdmComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  WidevineCdmComponentInstallerTraits();
  ~WidevineCdmComponentInstallerTraits() override {}

 private:
  // The following methods override ComponentInstallerTraits.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  bool VerifyInstallation(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  // Checks and updates CDM adapter if necessary to make sure the latest CDM
  // adapter is always used.
  // Note: The component is ready when CDM is present, but the CDM won't be
  // registered until the adapter is copied by this function (see
  // VerifyInstallation).
  void UpdateCdmAdapter(const base::Version& cdm_version,
                        const base::FilePath& cdm_install_dir,
                        std::unique_ptr<base::DictionaryValue> manifest);

  DISALLOW_COPY_AND_ASSIGN(WidevineCdmComponentInstallerTraits);
};

WidevineCdmComponentInstallerTraits::WidevineCdmComponentInstallerTraits() {
}

bool WidevineCdmComponentInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool WidevineCdmComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
WidevineCdmComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

// Once the CDM is ready, check the CDM adapter.
void WidevineCdmComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (!IsCompatibleWithChrome(*manifest)) {
    VLOG(1) << "Installed Widevine CDM component is incompatible.";
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&WidevineCdmComponentInstallerTraits::UpdateCdmAdapter,
                 base::Unretained(this), version, path,
                 base::Passed(&manifest)));
}

bool WidevineCdmComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return IsCompatibleWithChrome(manifest) &&
         base::PathExists(GetPlatformDirectory(install_dir)
                              .AppendASCII(base::GetNativeLibraryName(
                                  kWidevineCdmLibraryName)));
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\.
base::FilePath WidevineCdmComponentInstallerTraits::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("WidevineCdm"));
}

void WidevineCdmComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kSha2Hash, kSha2Hash + arraysize(kSha2Hash));
}

std::string WidevineCdmComponentInstallerTraits::GetName() const {
  return kWidevineCdmDisplayName;
}

update_client::InstallerAttributes
WidevineCdmComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> WidevineCdmComponentInstallerTraits::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

static bool HasValidAdapter(const base::FilePath& adapter_version_path,
                            const base::FilePath& adapter_install_path,
                            const std::string& chrome_version) {
  std::string adapter_version;
  return base::ReadFileToString(adapter_version_path, &adapter_version) &&
         adapter_version == chrome_version &&
         base::PathExists(adapter_install_path);
}

void WidevineCdmComponentInstallerTraits::UpdateCdmAdapter(
    const base::Version& cdm_version,
    const base::FilePath& cdm_install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // On some platforms (e.g. Mac) we use symlinks for paths. Since we are
  // comparing paths below, convert paths to absolute paths to avoid unexpected
  // failure. base::MakeAbsoluteFilePath() requires IO so it can only be done
  // in this function.
  const base::FilePath absolute_cdm_install_dir =
      base::MakeAbsoluteFilePath(cdm_install_dir);
  if (absolute_cdm_install_dir.empty()) {
    PLOG(WARNING) << "Failed to get absolute CDM install path.";
    return;
  }

  const base::FilePath adapter_version_path =
      GetPlatformDirectory(absolute_cdm_install_dir)
          .AppendASCII(kCdmAdapterVersionName);
  const base::FilePath adapter_install_path =
      GetPlatformDirectory(absolute_cdm_install_dir)
          .AppendASCII(kWidevineCdmAdapterFileName);

  VLOG(1) << "UpdateCdmAdapter: version" << cdm_version.GetString();
  VLOG(1) << " - adapter_install_path=" << adapter_install_path.AsUTF8Unsafe();
  VLOG(1) << " - adapter_version_path=" << adapter_version_path.AsUTF8Unsafe();

  base::FilePath adapter_source_path;
  PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
  adapter_source_path = base::MakeAbsoluteFilePath(adapter_source_path);
  if (adapter_source_path.empty()) {
    PLOG(WARNING) << "Failed to get absolute adapter source path.";
    return;
  }

  const std::string chrome_version = version_info::GetVersionNumber();
  DCHECK(!chrome_version.empty());

  // If we are not using bundled CDM and we don't have a valid adapter, create
  // the version file, copy the CDM adapter signature file, and copy the CDM
  // adapter.
  if (adapter_install_path != adapter_source_path &&
      !HasValidAdapter(adapter_version_path, adapter_install_path,
                       chrome_version)) {
    if (!base::CopyFile(adapter_source_path, adapter_install_path)) {
      PLOG(WARNING) << "Failed to copy Widevine CDM adapter.";
      return;
    }

    // Generate the version file.
    int bytes_written = base::WriteFile(
        adapter_version_path, chrome_version.data(), chrome_version.size());
    if (bytes_written < 0 ||
        static_cast<size_t>(bytes_written) != chrome_version.size()) {
      PLOG(WARNING) << "Failed to write Widevine CDM adapter version file.";
      // Ignore version file writing failure.
    }

    // Copy Widevine CDM adapter signature file.
    if (!base::CopyFile(
            adapter_source_path.AddExtension(kSignatureFileExtension),
            adapter_install_path.AddExtension(kSignatureFileExtension))) {
      PLOG(WARNING) << "Failed to copy Widevine CDM adapter signature file.";
      // The sig file may be missing or the copy failed. Ignore the failure.
    }
  }

  BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&RegisterWidevineCdmWithChrome, cdm_version,
                 absolute_cdm_install_dir, base::Passed(&manifest)));
}

#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

void RegisterWidevineCdmComponent(ComponentUpdateService* cus) {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  base::FilePath adapter_source_path;
  PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
  if (!base::PathExists(adapter_source_path))
    return;
  std::unique_ptr<ComponentInstallerTraits> traits(
      new WidevineCdmComponentInstallerTraits);
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
}

}  // namespace component_updater
