// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/flash_component_installer.h"

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
#include "chrome/common/pepper_flash.h"
#include "chrome/common/pepper_flash.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/ppapi/plugin_module.h"

#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.

using content::BrowserThread;
using content::PluginService;

namespace {

// CRX hash. The extension id is: mimojjlkmoijpicakmndhoigimigcmbb.
const uint8 sha2_hash[] = {0xc8, 0xce, 0x99, 0xba, 0xce, 0x89, 0xf8, 0x20,
                           0xac, 0xd3, 0x7e, 0x86, 0x8c, 0x86, 0x2c, 0x11,
                           0xb9, 0x40, 0xc5, 0x55, 0xaf, 0x08, 0x63, 0x70,
                           0x54, 0xf9, 0x56, 0xd3, 0xe7, 0x88, 0xba, 0x8c};

// File name of the Pepper Flash component manifest on different platforms.
const char kPepperFlashManifestName[] = "Flapper";

// Name of the Pepper Flash OS in the component manifest.
const char kPepperFlashOperatingSystem[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#else  // OS_LINUX, etc. TODO(viettrungluu): Separate out Chrome OS and Android?
    "linux";
#endif

// Name of the Pepper Flash architecture in the component manifest.
const char kPepperFlashArch[] =
#if defined(ARCH_CPU_X86)
    "ia32";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// If we don't have a Pepper Flash component, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\PepperFlash\.
base::FilePath GetPepperFlashBaseDirectory() {
  base::FilePath result;
  PathService::Get(chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN, &result);
  return result;
}

#if defined(GOOGLE_CHROME_BUILD)
// Pepper Flash plugins have the version encoded in the path itself
// so we need to enumerate the directories to find the full path.
// On success, |latest_dir| returns something like:
// <profile>\AppData\Local\Google\Chrome\User Data\PepperFlash\10.3.44.555\.
// |latest_version| returns the corresponding version number. |older_dirs|
// returns directories of all older versions.
bool GetPepperFlashDirectory(base::FilePath* latest_dir,
                             Version* latest_version,
                             std::vector<base::FilePath>* older_dirs) {
  base::FilePath base_dir = GetPepperFlashBaseDirectory();
  bool found = false;
  file_util::FileEnumerator
      file_enumerator(base_dir, false, file_util::FileEnumerator::DIRECTORIES);
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
#endif

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

bool MakePepperFlashPluginInfo(const base::FilePath& flash_path,
                               const Version& flash_version,
                               bool out_of_process,
                               content::PepperPluginInfo* plugin_info) {
  if (!flash_version.IsValid())
    return false;
  const std::vector<uint16> ver_nums = flash_version.components();
  if (ver_nums.size() < 3)
    return false;

  plugin_info->is_internal = false;
  plugin_info->is_out_of_process = out_of_process;
  plugin_info->path = flash_path;
  plugin_info->name = kFlashPluginName;
  plugin_info->permissions = kPepperFlashPermissions;

  // The description is like "Shockwave Flash 10.2 r154".
  plugin_info->description = StringPrintf("%s %d.%d r%d",
      kFlashPluginName, ver_nums[0], ver_nums[1], ver_nums[2]);

  plugin_info->version = flash_version.GetString();

  webkit::WebPluginMimeType swf_mime_type(kFlashPluginSwfMimeType,
                                          kFlashPluginSwfExtension,
                                          kFlashPluginName);
  plugin_info->mime_types.push_back(swf_mime_type);
  webkit::WebPluginMimeType spl_mime_type(kFlashPluginSplMimeType,
                                          kFlashPluginSplExtension,
                                          kFlashPluginName);
  plugin_info->mime_types.push_back(spl_mime_type);
  return true;
}

bool IsPepperFlash(const webkit::WebPluginInfo& plugin) {
  // We try to recognize Pepper Flash by the following criteria:
  // * It is a Pepper plug-in.
  // * It has the special Flash permissions.
  return webkit::IsPepperPlugin(plugin) &&
         (plugin.pepper_permissions & ppapi::PERMISSION_FLASH);
}

void RegisterPepperFlashWithChrome(const base::FilePath& path,
                                   const Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::PepperPluginInfo plugin_info;
  if (!MakePepperFlashPluginInfo(path, version, true, &plugin_info))
    return;

  std::vector<webkit::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);
  for (std::vector<webkit::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    if (!IsPepperFlash(*it))
      continue;

    // Do it only if the version we're trying to register is newer.
    Version registered_version(UTF16ToUTF8(it->version));
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) <= 0) {
      return;
    }

    // If the version is newer, remove the old one first.
    PluginService::GetInstance()->UnregisterInternalPlugin(it->path);
    break;
  }

  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  PluginService::GetInstance()->RefreshPlugins();
}

// Returns true if this browser implements one of the interfaces given in
// |interface_string|, which is a '|'-separated string of interface names.
bool CheckPepperFlashInterfaceString(const std::string& interface_string) {
  std::vector<std::string> interface_names;
  base::SplitString(interface_string, '|', &interface_names);
  for (size_t i = 0; i < interface_names.size(); i++) {
    if (SupportsPepperInterface(interface_names[i].c_str()))
      return true;
  }
  return false;
}

// Returns true if this browser implements all the interfaces that Flash
// specifies in its component installer manifest.
bool CheckPepperFlashInterfaces(base::DictionaryValue* manifest) {
  base::ListValue* interface_list = NULL;

  // We don't *require* an interface list, apparently.
  if (!manifest->GetList("x-ppapi-required-interfaces", &interface_list))
    return true;

  for (size_t i = 0; i < interface_list->GetSize(); i++) {
    std::string interface_string;
    if (!interface_list->GetString(i, &interface_string))
      return false;
    if (!CheckPepperFlashInterfaceString(interface_string))
      return false;
  }

  return true;
}

}  // namespace

class PepperFlashComponentInstaller : public ComponentInstaller {
 public:
  explicit PepperFlashComponentInstaller(const Version& version);

  virtual ~PepperFlashComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(base::DictionaryValue* manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

 private:
  Version current_version_;
};

PepperFlashComponentInstaller::PepperFlashComponentInstaller(
    const Version& version) : current_version_(version) {
  DCHECK(version.IsValid());
}

void PepperFlashComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Pepper Flash update error: " << error;
}

bool PepperFlashComponentInstaller::Install(base::DictionaryValue* manifest,
                                            const base::FilePath& unpack_path) {
  Version version;
  if (!CheckPepperFlashManifest(manifest, &version))
    return false;
  if (current_version_.CompareTo(version) > 0)
    return false;
  if (!file_util::PathExists(unpack_path.Append(
          chrome::kPepperFlashPluginFilename)))
    return false;
  // Passed the basic tests. Time to install it.
  base::FilePath path =
      GetPepperFlashBaseDirectory().AppendASCII(version.GetString());
  if (file_util::PathExists(path))
    return false;
  if (!file_util::Move(unpack_path, path))
    return false;
  // Installation is done. Now tell the rest of chrome. Both the path service
  // and to the plugin service.
  current_version_ = version;
  PathService::Override(chrome::DIR_PEPPER_FLASH_PLUGIN, path);
  path = path.Append(chrome::kPepperFlashPluginFilename);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RegisterPepperFlashWithChrome, path, version));
  return true;
}

bool CheckPepperFlashManifest(base::DictionaryValue* manifest,
                              Version* version_out) {
  std::string name;
  manifest->GetStringASCII("name", &name);
  // TODO(viettrungluu): Support WinFlapper for now, while we change the format
  // of the manifest. (Should be safe to remove checks for "WinFlapper" in, say,
  // Nov. 2011.)  crbug.com/98458
  if (name != kPepperFlashManifestName && name != "WinFlapper")
    return false;

  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;

  if (!CheckPepperFlashInterfaces(manifest))
    return false;

  // TODO(viettrungluu): See above TODO.
  if (name == "WinFlapper") {
    *version_out = version;
    return true;
  }

  std::string os;
  manifest->GetStringASCII("x-ppapi-os", &os);
  if (os != kPepperFlashOperatingSystem)
    return false;

  std::string arch;
  manifest->GetStringASCII("x-ppapi-arch", &arch);
  if (arch != kPepperFlashArch)
    return false;

  *version_out = version;
  return true;
}

namespace {

#if defined(GOOGLE_CHROME_BUILD)
void FinishPepperFlashUpdateRegistration(ComponentUpdateService* cus,
                                         const Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CrxComponent pepflash;
  pepflash.name = "pepper_flash";
  pepflash.installer = new PepperFlashComponentInstaller(version);
  pepflash.version = version;
  pepflash.pk_hash.assign(sha2_hash, &sha2_hash[sizeof(sha2_hash)]);
  if (cus->RegisterComponent(pepflash) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Pepper Flash component registration failed.";
  }
}

void StartPepperFlashUpdateRegistration(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = GetPepperFlashBaseDirectory();
  if (!file_util::PathExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      NOTREACHED() << "Could not create Pepper Flash directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetPepperFlashDirectory(&path, &version, &older_dirs)) {
    path = path.Append(chrome::kPepperFlashPluginFilename);
    if (file_util::PathExists(path)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&RegisterPepperFlashWithChrome, path, version));
    } else {
      version = Version(kNullVersion);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishPepperFlashUpdateRegistration, cus, version));

  // Remove older versions of Pepper Flash.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    file_util::Delete(*iter, true);
  }
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

void RegisterPepperFlashComponent(ComponentUpdateService* cus) {
#if defined(GOOGLE_CHROME_BUILD)
  // Component updated flash supersedes bundled flash therefore if that one
  // is disabled then this one should never install.
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBundledPpapiFlash))
    return;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&StartPepperFlashUpdateRegistration, cus));
#endif
}
