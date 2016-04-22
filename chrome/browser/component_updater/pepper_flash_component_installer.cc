// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pepper_flash_component_installer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/ppapi_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.  NOLINT
#include "ppapi/shared_impl/ppapi_permissions.h"

#if defined(OS_LINUX)
#include "chrome/common/component_flash_hint_file_linux.h"
#endif  // defined(OS_LINUX)

using content::BrowserThread;
using content::PluginService;

namespace component_updater {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
// CRX hash. The extension id is: mimojjlkmoijpicakmndhoigimigcmbb.
const uint8_t kSha2Hash[] = {0xc8, 0xce, 0x99, 0xba, 0xce, 0x89, 0xf8, 0x20,
                             0xac, 0xd3, 0x7e, 0x86, 0x8c, 0x86, 0x2c, 0x11,
                             0xb9, 0x40, 0xc5, 0x55, 0xaf, 0x08, 0x63, 0x70,
                             0x54, 0xf9, 0x56, 0xd3, 0xe7, 0x88, 0xba, 0x8c};

// If we don't have a Pepper Flash component, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// Pepper Flash plugins have the version encoded in the path itself
// so we need to enumerate the directories to find the full path.
// On success, |latest_dir| returns something like:
// <profile>\AppData\Local\Google\Chrome\User Data\PepperFlash\10.3.44.555\.
// |latest_version| returns the corresponding version number. |older_dirs|
// returns directories of all older versions.
bool GetPepperFlashDirectory(base::FilePath* latest_dir,
                             Version* latest_version,
                             std::vector<base::FilePath>* older_dirs) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath base_dir;
  if (!PathService::Get(chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN,
                        &base_dir)) {
    return false;
  }

  bool found = false;
  base::FileEnumerator file_enumerator(
      base_dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
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
#endif  // defined(GOOGLE_CHROME_BUILD)

#if !defined(OS_LINUX) || defined(GOOGLE_CHROME_BUILD)
bool MakePepperFlashPluginInfo(const base::FilePath& flash_path,
                               const Version& flash_version,
                               bool out_of_process,
                               content::PepperPluginInfo* plugin_info) {
  if (!flash_version.IsValid())
    return false;
  const std::vector<uint32_t> ver_nums = flash_version.components();
  if (ver_nums.size() < 3)
    return false;

  plugin_info->is_internal = false;
  plugin_info->is_out_of_process = out_of_process;
  plugin_info->path = flash_path;
  plugin_info->name = content::kFlashPluginName;
  plugin_info->permissions = chrome::kPepperFlashPermissions;

  // The description is like "Shockwave Flash 10.2 r154".
  plugin_info->description = base::StringPrintf("%s %d.%d r%d",
                                                content::kFlashPluginName,
                                                ver_nums[0],
                                                ver_nums[1],
                                                ver_nums[2]);

  plugin_info->version = flash_version.GetString();

  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(spl_mime_type);
  return true;
}

bool IsPepperFlash(const content::WebPluginInfo& plugin) {
  // We try to recognize Pepper Flash by the following criteria:
  // * It is a Pepper plugin.
  // * It has the special Flash permissions.
  return plugin.is_pepper_plugin() &&
         (plugin.pepper_permissions & ppapi::PERMISSION_FLASH);
}

void RegisterPepperFlashWithChrome(const base::FilePath& path,
                                   const Version& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::PepperPluginInfo plugin_info;
  if (!MakePepperFlashPluginInfo(path, version, true, &plugin_info))
    return;

  base::FilePath bundled_flash_dir;
  PathService::Get(chrome::DIR_PEPPER_FLASH_PLUGIN, &bundled_flash_dir);
  base::FilePath system_flash_path;
  PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN, &system_flash_path);

  std::vector<content::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);
  for (const auto& plugin : plugins) {
    if (!IsPepperFlash(plugin))
      continue;

    Version registered_version(base::UTF16ToUTF8(plugin.version));

    // If lower version, never register.
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) < 0) {
      return;
    }

    bool registered_is_bundled =
        !bundled_flash_dir.empty() && bundled_flash_dir.IsParent(plugin.path);
    bool registered_is_debug_system =
        !system_flash_path.empty() &&
        base::FilePath::CompareEqualIgnoreCase(plugin.path.value(),
                                               system_flash_path.value()) &&
        chrome::IsSystemFlashScriptDebuggerPresent();
    bool is_on_network = false;
#if defined(OS_WIN)
    // On Windows, component updated DLLs can't load off network drives.
    // See crbug.com/572131 for details.
    is_on_network = base::IsOnNetworkDrive(path);
#endif
    // If equal version, register iff component is not on a network drive,
    // and the version of flash is not bundled, and not debug system.
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) == 0 &&
        (is_on_network || registered_is_bundled ||
         registered_is_debug_system)) {
      return;
    }

    // If the version is newer, remove the old one first.
    PluginService::GetInstance()->UnregisterInternalPlugin(plugin.path);
    break;
  }

  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  PluginService::GetInstance()->RefreshPlugins();
}
#endif  // !defined(OS_LINUX) || defined(GOOGLE_CHROME_BUILD)

}  // namespace

class PepperFlashComponentInstaller : public update_client::CrxInstaller {
 public:
  explicit PepperFlashComponentInstaller(const Version& version);

  // ComponentInstaller implementation:
  void OnUpdateError(int error) override;

  bool Install(const base::DictionaryValue& manifest,
               const base::FilePath& unpack_path) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

  bool Uninstall() override;

 private:
  ~PepperFlashComponentInstaller() override {}

  Version current_version_;
};

PepperFlashComponentInstaller::PepperFlashComponentInstaller(
    const Version& version)
    : current_version_(version) {
  DCHECK(version.IsValid());
}

void PepperFlashComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Pepper Flash update error: " << error;
}

bool PepperFlashComponentInstaller::Install(
    const base::DictionaryValue& manifest,
    const base::FilePath& unpack_path) {
  Version version;
  if (!chrome::CheckPepperFlashManifest(manifest, &version))
    return false;
  if (current_version_.CompareTo(version) > 0)
    return false;
  const base::FilePath unpacked_plugin =
      unpack_path.Append(chrome::kPepperFlashPluginFilename);
  if (!base::PathExists(unpacked_plugin))
    return false;
  // Passed the basic tests. Time to install it.
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN,
                        &path)) {
    return false;
  }
  path = path.AppendASCII(version.GetString());
  if (base::PathExists(path))
    return false;
  current_version_ = version;

  if (!base::Move(unpack_path, path))
    return false;
#if defined(OS_LINUX)
  const base::FilePath flash_path =
      path.Append(chrome::kPepperFlashPluginFilename);
  // Populate the component updated flash hint file so that the zygote can
  // locate and preload the latest version of flash.
  if (!component_flash_hint_file::RecordFlashUpdate(flash_path, flash_path,
                                                    version.GetString())) {
    if (!base::DeleteFile(path, true))
      LOG(ERROR) << "Hint file creation failed, but unable to delete "
                    "installed flash plugin.";
    return false;
  }
#else
  // Installation is done. Now tell the rest of chrome. Both the path service
  // and to the plugin service. On Linux, a restart is required to use the new
  // Flash version, so we do not do this.
  PathService::Override(chrome::DIR_PEPPER_FLASH_PLUGIN, path);
  path = path.Append(chrome::kPepperFlashPluginFilename);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RegisterPepperFlashWithChrome, path, version));
#endif  // !defined(OS_LINUX)
  return true;
}

bool PepperFlashComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  return false;
}

bool PepperFlashComponentInstaller::Uninstall() {
  return false;
}



namespace {

#if defined(GOOGLE_CHROME_BUILD)
void FinishPepperFlashUpdateRegistration(ComponentUpdateService* cus,
                                         const Version& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  update_client::CrxComponent pepflash;
  pepflash.name = "pepper_flash";
  pepflash.installer = new PepperFlashComponentInstaller(version);
  pepflash.version = version;
  pepflash.pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);
  if (!cus->RegisterComponent(pepflash))
    NOTREACHED() << "Pepper Flash component registration failed.";
}

bool ValidatePepperFlashManifest(const base::FilePath& path) {
  base::FilePath manifest_path(path.DirName().AppendASCII("manifest.json"));

  std::string manifest_data;
  if (!base::ReadFileToString(manifest_path, &manifest_data))
    return false;
  std::unique_ptr<base::Value> manifest_value(
      base::JSONReader::Read(manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!manifest_value.get())
    return false;
  base::DictionaryValue* manifest = NULL;
  if (!manifest_value->GetAsDictionary(&manifest))
    return false;
  Version version;
  if (!chrome::CheckPepperFlashManifest(*manifest, &version))
    return false;
  return true;
}

void StartPepperFlashUpdateRegistration(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN,
                        &path)) {
    return;
  }

  if (!base::PathExists(path)) {
    if (!base::CreateDirectory(path)) {
      NOTREACHED() << "Could not create Pepper Flash directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetPepperFlashDirectory(&path, &version, &older_dirs)) {
    path = path.Append(chrome::kPepperFlashPluginFilename);
    if (base::PathExists(path)) {
      // Only register component pepper flash if it validates manifest check.
      if (ValidatePepperFlashManifest(path)) {
        BrowserThread::PostTask(
            BrowserThread::UI,
            FROM_HERE,
            base::Bind(&RegisterPepperFlashWithChrome, path, version));
      } else {
        // Queue this version to be deleted.
        older_dirs.push_back(path.DirName());
        version = Version(kNullVersion);
      }
    } else {
      version = Version(kNullVersion);
    }
  }

#if defined(FLAPPER_AVAILABLE)
  // If a version of Flash is bundled with Chrome, and it's a higher version
  // than the version of the component, or the component has never been updated,
  // then set the bundled version as the current version.
  if (version.CompareTo(Version(FLAPPER_VERSION_STRING)) < 0)
    version = Version(FLAPPER_VERSION_STRING);
#endif

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&FinishPepperFlashUpdateRegistration, cus, version));

  // Remove older versions of Pepper Flash.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end();
       ++iter) {
    base::DeleteFile(*iter, true);
  }
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

void RegisterPepperFlashComponent(ComponentUpdateService* cus) {
#if defined(GOOGLE_CHROME_BUILD)
  // Component updated flash supersedes bundled flash therefore if that one
  // is disabled then this one should never install.
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBundledPpapiFlash))
    return;
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&StartPepperFlashUpdateRegistration, cus));
#endif  // defined(GOOGLE_CHROME_BUILD)
}

}  // namespace component_updater
