// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

#if defined(OS_CHROMEOS)
#include "base/file_util.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#endif

using content::BrowserThread;

namespace {

extensions::ComponentLoader* GetComponentLoader(Profile* profile) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}

void LoadGaiaAuthExtension(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  extensions::ComponentLoader* component_loader = GetComponentLoader(profile);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
    base::FilePath auth_extension_path =
        command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
    component_loader->Add(IDR_GAIA_AUTH_MANIFEST, auth_extension_path);
    return;
  }

#if defined(OS_CHROMEOS)
  if (command_line->HasSwitch(chromeos::switches::kGAIAAuthExtensionManifest)) {
    const base::FilePath manifest_path = command_line->GetSwitchValuePath(
        chromeos::switches::kGAIAAuthExtensionManifest);
    std::string manifest;
    if (!base::ReadFileToString(manifest_path, &manifest))
      NOTREACHED();
    component_loader->Add(manifest,
                          base::FilePath(FILE_PATH_LITERAL("gaia_auth")));
    return;
  }

  int manifest_resource_id = IDR_GAIA_AUTH_MANIFEST;
  if (chromeos::system::InputDeviceSettings::Get()
          ->ForceKeyboardDrivenUINavigation()) {
    manifest_resource_id = IDR_GAIA_AUTH_KEYBOARD_MANIFEST;
  } else if (!command_line->HasSwitch(chromeos::switches::kDisableSamlSignin)) {
    manifest_resource_id = IDR_GAIA_AUTH_SAML_MANIFEST;
  }
#else
  int manifest_resource_id = IDR_GAIA_AUTH_DESKTOP_MANIFEST;
#endif

  component_loader->Add(manifest_resource_id,
                        base::FilePath(FILE_PATH_LITERAL("gaia_auth")));
}

void UnloadGaiaAuthExtension(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const char kGaiaAuthId[] = "mfffpogegjflfpflabcdkioaeobkgjik";
  GetComponentLoader(profile)->Remove(kGaiaAuthId);
}

}  // namespace

namespace extensions {

GaiaAuthExtensionLoader::GaiaAuthExtensionLoader(Profile* profile)
    : profile_(profile), load_count_(0) {}

GaiaAuthExtensionLoader::~GaiaAuthExtensionLoader() {
  DCHECK_EQ(0, load_count_);
}

void GaiaAuthExtensionLoader::LoadIfNeeded() {
  if (load_count_ == 0)
    LoadGaiaAuthExtension(profile_);
  ++load_count_;
}

void GaiaAuthExtensionLoader::UnloadIfNeeded() {
  --load_count_;
  if (load_count_ == 0)
    UnloadGaiaAuthExtension(profile_);
}

void GaiaAuthExtensionLoader::Shutdown() {
  if (load_count_ > 0) {
    UnloadGaiaAuthExtension(profile_);
    load_count_ = 0;
  }
}

// static
GaiaAuthExtensionLoader* GaiaAuthExtensionLoader::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<GaiaAuthExtensionLoader>::GetForProfile(
      profile);
}

static base::LazyInstance<ProfileKeyedAPIFactory<GaiaAuthExtensionLoader> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<GaiaAuthExtensionLoader>*
GaiaAuthExtensionLoader::GetFactoryInstance() {
  return g_factory.Pointer();
}

} // namespace extensions
