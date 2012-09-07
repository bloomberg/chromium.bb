// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/flash_component_installer.h"

#include <algorithm>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "webkit/plugins/webplugininfo.h"

using content::BrowserThread;
using content::PluginService;

namespace {

// CRX hash. The extension id is: immdilkhigodmjbnngapbehchmihabbg.
const uint8 sha2_hash[] = {0x8c, 0xc3, 0x8b, 0xa7, 0x86, 0xe3, 0xc9, 0x1d,
                           0xd6, 0x0f, 0x14, 0x72, 0x7c, 0x87, 0x01, 0x16,
                           0xe2, 0x00, 0x6c, 0x98, 0xbc, 0xfb, 0x14, 0x1b,
                           0x5c, 0xcd, 0xff, 0x3d, 0xa3, 0x2e, 0x2c, 0x49};

// File name of the internal Flash plugin on different platforms.
const FilePath::CharType kFlashPluginFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("Flash Player Plugin for Chrome.plugin");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("gcswf32.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libgcflashplayer.so");
#endif

const char kNPAPIFlashManifestName[] = "NPAPIFlash";

// The NPAPI flash plugins are in a directory with this name.
const FilePath::CharType kNPAPIFlashBaseDirectory[] =
    FILE_PATH_LITERAL("NPAPIFlash");

// The base directory on windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\NPAPIFlash\.
FilePath GetNPAPIFlashBaseDirectory() {
  FilePath result;
  PathService::Get(chrome::DIR_USER_DATA, &result);
  return result.Append(kNPAPIFlashBaseDirectory);
}

std::string NormalizeVersion(const string16& version) {
  std::string ascii_ver = UTF16ToASCII(version);
  std::replace(ascii_ver.begin(), ascii_ver.end(), ',', '.');
  return ascii_ver;
}

}  // namespace

class NPAPIFlashComponentInstaller : public ComponentInstaller {
 public:
  explicit NPAPIFlashComponentInstaller(const Version& version);

  virtual ~NPAPIFlashComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(base::DictionaryValue* manifest,
                       const FilePath& unpack_path) OVERRIDE;

 private:
  Version current_version_;
};

NPAPIFlashComponentInstaller::NPAPIFlashComponentInstaller(
    const Version& version) : current_version_(version) {
  DCHECK(version.IsValid());
}

void NPAPIFlashComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "NPAPI flash update error: " << error;
}

bool NPAPIFlashComponentInstaller::Install(base::DictionaryValue* manifest,
                                           const FilePath& unpack_path) {
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kNPAPIFlashManifestName)
    return false;
  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) >= 0)
    return false;
  if (!file_util::PathExists(unpack_path.Append(kFlashPluginFileName)))
    return false;
  // Passed the basic tests. Time to install it.
  if (!file_util::Move(unpack_path, GetNPAPIFlashBaseDirectory()))
    return false;
  // Installation is done. Now tell the rest of chrome.
  current_version_ = version;
  PluginService::GetInstance()->RefreshPlugins();
  return true;
}

void FinishFlashUpdateRegistration(ComponentUpdateService* cus,
                                   const webkit::WebPluginInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // todo(cpu): Add PluginPrefs code here to determine if what we update
  // is actually going to be used.

  Version version(NormalizeVersion(info.version));
  if (!version.IsValid()) {
    NOTREACHED();
    return;
  }

  CrxComponent flash;
  flash.name = "npapi_flash";
  flash.installer = new NPAPIFlashComponentInstaller(version);
  flash.version = version;
  flash.pk_hash.assign(sha2_hash, &sha2_hash[sizeof(sha2_hash)]);
  if (cus->RegisterComponent(flash) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Flash component registration fail";
  }
}

// The code in this function is only concerned about learning what flash plugin
// chrome is using and what is its version. This will determine if we register
// for component update or not. Read the comments on RegisterNPAPIFlashComponent
// for more background.
void StartFlashUpdateRegistration(ComponentUpdateService* cus,
                                  const std::vector<webkit::WebPluginInfo>&) {
  FilePath builtin_plugin_path;
  if (!PathService::Get(chrome::FILE_FLASH_PLUGIN_EXISTING,
      &builtin_plugin_path))
    return;

  FilePath updated_plugin_path =
      GetNPAPIFlashBaseDirectory().Append(kFlashPluginFileName);

  PluginService* plugins = PluginService::GetInstance();
  webkit::WebPluginInfo plugin_info;

  if (plugins->GetPluginInfoByPath(updated_plugin_path, &plugin_info)) {
    // The updated plugin is newer. Since flash is used by pretty much every
    // webpage out there, odds are it is going to be loaded, which means
    // receiving an update is pointless since we can't swap them. You might
    // find this pessimistic. The way we get out of this situation is when
    // we update the whole product.
    DLOG(INFO) << "updated plugin overriding built-in flash";
    return;
  } else if (plugins->GetPluginInfoByPath(builtin_plugin_path, &plugin_info)) {
    // The built plugin is newer. Delete the updated plugin and register for
    // updates. This should be the normal case.
    file_util::Delete(GetNPAPIFlashBaseDirectory(), true);
  } else {
    // Strange installation. Log and abort registration.
    DLOG(WARNING) << "Strange flash npapi configuration";
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishFlashUpdateRegistration, cus, plugin_info));
}

// Here is the general plan of action: we are going to update flash and we have
// the following cases:
// 1- The active flash is not the built-in flash (user override).
// 2- The active flash is the built-in flash.
// 3- The active flash is the one from the component updater.
// In case 1 we do nothing. In cases 2 and 3 we need to compare versions
// and if the higher version is:
// case 2: remove the component update flash, register with component updater.
// case 3: register with component updater.
//
// In practice, it's complicated. First off, to learn the version of the plugin
// we have to do file IO, which PluginList will do for us but we have to be
// careful not to trigger this on the UI thread: AddExtraPluginPath and
// RefreshPlugins don't trigger file IO, but most of the others calls do.
//
// Secondly, we can do this in a delayed task. Right now we just need to
// register the right plugin before chrome loads the first flash-ladden page.
//
// Interestingly, if PluginList finds two plugins with the same filename and
// same mimetypes, it will only keep the one with the higher version. This is
// our case in the sense that a component updated flash is very much the same
// thing as built-in flash: sometimes newer or sometimes older. That is why
// you don't see version comparison in this code.
//
// So to kick off the magic we add the updated flash path here, even though
// there might not be a flash player in that location. If there is and it is
// newer, it will take its place then we fire StartFlashUpdateRegistration
// on the IO thread to learn which one won. Since we do it in a delayed task
// probably somebody (unknowingly) will pay for the file IO, so usually get
// the information for free.
void RegisterNPAPIFlashComponent(ComponentUpdateService* cus) {
#if !defined(OS_CHROMEOS)
  FilePath path = GetNPAPIFlashBaseDirectory().Append(kFlashPluginFileName);
  PluginService::GetInstance()->AddExtraPluginPath(path);
  PluginService::GetInstance()->RefreshPlugins();

  // Post the task to the FILE thread because IO may be done once the plugins
  // are loaded.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&PluginService::GetPlugins,
                 base::Unretained(PluginService::GetInstance()),
                 base::Bind(&StartFlashUpdateRegistration, cus)),
      base::TimeDelta::FromSeconds(8));
#endif
}
