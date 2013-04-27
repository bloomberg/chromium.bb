// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"

#include <string.h>

#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/ppapi/plugin_module.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using content::BrowserThread;
using content::PluginService;

namespace {

// TODO(xhwang): Move duplicate code among all component installer
// implementations to some common place.

// TODO(xhwang): This is the sha256sum of "Widevine CDM". Get the real extension
// ID for Widevine CDM.
// CRX hash. The extension id is: lgafhhiijclkaikclkjjofekikijofcm.
const uint8 kSha2Hash[] = {0xb6, 0x05, 0x77, 0x88, 0x92, 0xba, 0x08, 0xa2,
                           0xba, 0x99, 0xe5, 0x4a, 0x8a, 0x89, 0xe5, 0x2c,
                           0x88, 0x38, 0x2f, 0xf5, 0xa7, 0x7b, 0x93, 0xe7,
                           0xf1, 0x84, 0xcc, 0x37, 0xe1, 0xe5, 0x7a, 0xbd};

// File name of the Widevine CDM component manifest on different platforms.
const char kWidevineCdmManifestName[] = "WidevineCdm";

// Name of the Widevine CDM OS in the component manifest.
const char kWidevineCdmOperatingSystem[] =
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
    "ia32";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// If we don't have a Widevine CDM component, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\.
base::FilePath GetWidevineCdmBaseDirectory() {
  base::FilePath result;
  PathService::Get(chrome::DIR_WIDEVINE_CDM, &result);
  return result;
}

#if defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)
// Widevine CDM plugins have the version encoded in the path itself
// so we need to enumerate the directories to find the full path.
// On success, |latest_dir| returns something like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\10.3.44.555\.
// |latest_version| returns the corresponding version number. |older_dirs|
// returns directories of all older versions.
bool GetWidevineCdmDirectory(base::FilePath* latest_dir,
                             base::Version* latest_version,
                             std::vector<base::FilePath>* older_dirs) {
  base::FilePath base_dir = GetWidevineCdmBaseDirectory();
  bool found = false;
  file_util::FileEnumerator file_enumerator(
      base_dir, false, file_util::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (found) {
      if (version.CompareTo(*latest_version) > 0) {
        older_dirs->push_back(*latest_dir);
        *latest_dir = path;
        *latest_version = version;
      } else {
        older_dirs->push_back(path);
      }
    } else {
      *latest_dir = path;
      *latest_version = version;
      found = true;
    }
  }
  return found;
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)

// Returns true if the Pepper |interface_name| is implemented  by this browser.
// It does not check if the interface is proxied.
bool SupportsPepperInterface(const char* interface_name) {
  if (webkit::ppapi::PluginModule::SupportsInterface(interface_name))
    return true;
  // The PDF interface is invisible to SupportsInterface() on the browser
  // process because it is provided using PpapiInterfaceFactoryManager. We need
  // to check for that as well.
  // TODO(cpu): make this more sane.
  return (strcmp(interface_name, PPB_PDF_INTERFACE) == 0);
}

bool MakeWidevineCdmPluginInfo(const base::FilePath& path,
                               const base::Version& version,
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
  plugin_info->name = kWidevineCdmPluginName;
  plugin_info->description = kWidevineCdmPluginDescription;
  plugin_info->version = version.GetString();
  webkit::WebPluginMimeType widevine_cdm_mime_type(
      kWidevineCdmPluginMimeType,
      kWidevineCdmPluginExtension,
      kWidevineCdmPluginMimeTypeDescription);
  plugin_info->mime_types.push_back(widevine_cdm_mime_type);
  plugin_info->permissions = kWidevineCdmPluginPermissions;

  return true;
}

void RegisterWidevineCdmWithChrome(const base::FilePath& path,
                                   const base::Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::PepperPluginInfo plugin_info;
  if (!MakeWidevineCdmPluginInfo(path, version, &plugin_info))
    return;

  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  PluginService::GetInstance()->RefreshPlugins();
}

// Returns true if this browser implements one of the interfaces given in
// |interface_string|, which is a '|'-separated string of interface names.
bool CheckWidevineCdmInterfaceString(const std::string& interface_string) {
  std::vector<std::string> interface_names;
  base::SplitString(interface_string, '|', &interface_names);
  for (size_t i = 0; i < interface_names.size(); i++) {
    if (SupportsPepperInterface(interface_names[i].c_str()))
      return true;
  }
  return false;
}

// Returns true if this browser implements all the interfaces that Widevine CDM
// specifies in its component installer manifest.
bool CheckWidevineCdmInterfaces(const base::DictionaryValue& manifest) {
  const base::ListValue* interface_list = NULL;

  // We don't *require* an interface list, apparently.
  if (!manifest.GetList("x-ppapi-required-interfaces", &interface_list))
    return true;

  for (size_t i = 0; i < interface_list->GetSize(); i++) {
    std::string interface_string;
    if (!interface_list->GetString(i, &interface_string))
      return false;
    if (!CheckWidevineCdmInterfaceString(interface_string))
      return false;
  }

  return true;
}

// Returns true if this browser is compatible with the given Widevine CDM
// manifest, with the version specified in the manifest in |version_out|.
bool CheckWidevineCdmManifest(const base::DictionaryValue& manifest,
                              base::Version* version_out) {
  std::string name;
  manifest.GetStringASCII("name", &name);

  if (name != kWidevineCdmManifestName)
    return false;

  std::string proposed_version;
  manifest.GetStringASCII("version", &proposed_version);
  base::Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;

  if (!CheckWidevineCdmInterfaces(manifest))
    return false;

  std::string os;
  manifest.GetStringASCII("x-ppapi-os", &os);
  if (os != kWidevineCdmOperatingSystem)
    return false;

  std::string arch;
  manifest.GetStringASCII("x-ppapi-arch", &arch);
  if (arch != kWidevineCdmArch)
    return false;

  *version_out = version;
  return true;
}

}  // namespace

class WidevineCdmComponentInstaller : public ComponentInstaller {
 public:
  explicit WidevineCdmComponentInstaller(const base::Version& version);
  virtual ~WidevineCdmComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;
  virtual bool Install(base::DictionaryValue* manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

 private:
  base::Version current_version_;
};

WidevineCdmComponentInstaller::WidevineCdmComponentInstaller(
    const base::Version& version)
    : current_version_(version) {
  DCHECK(version.IsValid());
}

void WidevineCdmComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Widevine CDM update error: " << error;
}

bool WidevineCdmComponentInstaller::Install(base::DictionaryValue* manifest,
                                            const base::FilePath& unpack_path) {
  base::Version version;
  if (!CheckWidevineCdmManifest(*manifest, &version))
    return false;
  if (current_version_.CompareTo(version) > 0)
    return false;

  // TODO(xhwang): Also check if Widevine CDM binary exists.
  if (!file_util::PathExists(unpack_path.Append(kWidevineCdmPluginFileName)))
    return false;

  // Passed the basic tests. Time to install it.
  base::FilePath path =
      GetWidevineCdmBaseDirectory().AppendASCII(version.GetString());
  if (file_util::PathExists(path))
    return false;
  if (!file_util::Move(unpack_path, path))
    return false;

  // Installation is done. Now tell the rest of chrome. Both the path service
  // and to the plugin service.
  current_version_ = version;
  PathService::Override(chrome::DIR_PEPPER_FLASH_PLUGIN, path);
  path = path.Append(kWidevineCdmPluginFileName);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&RegisterWidevineCdmWithChrome, path, version));
  return true;
}

namespace {

#if defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)
void StartWidevineCdmUpdateRegistration(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = GetWidevineCdmBaseDirectory();
  if (!file_util::PathExists(path) && !file_util::CreateDirectory(path)) {
    NOTREACHED() << "Could not create Widevine CDM directory.";
    return;
  }

  base::Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetWidevineCdmDirectory(&path, &version, &older_dirs)) {
    path = path.Append(kWidevineCdmPluginFileName);
    if (file_util::PathExists(path)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&RegisterWidevineCdmWithChrome, path, version));
    } else {
      version = base::Version(kNullVersion);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishWidevineCdmUpdateRegistration, cus, version));

  // Remove older versions of Widevine CDM.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    file_util::Delete(*iter, true);
  }
}

void FinishWidevineCdmUpdateRegistration(ComponentUpdateService* cus,
                                         const base::Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CrxComponent widevine_cdm;
  widevine_cdm.name = "WidevineCdm";
  widevine_cdm.installer = new WidevineCdmComponentInstaller(version);
  widevine_cdm.version = version;
  widevine_cdm.pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);
  if (cus->RegisterComponent(widevine_cdm) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Widevine CDM component registration failed.";
  }
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)

}  // namespace

void RegisterWidevineCdmComponent(ComponentUpdateService* cus) {
#if defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&StartWidevineCdmUpdateRegistration, cus));
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_LINUX)
}
