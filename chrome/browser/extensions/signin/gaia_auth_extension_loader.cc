// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/input_device_settings.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace {

extensions::ComponentLoader* GetComponentLoader(BrowserContext* context) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(context);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}

void LoadGaiaAuthExtension(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  extensions::ComponentLoader* component_loader = GetComponentLoader(context);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
    base::FilePath auth_extension_path =
        command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
    component_loader->Add(IDR_GAIA_AUTH_MANIFEST, auth_extension_path);
    return;
  }

  int manifest_resource_id = IDR_GAIA_AUTH_MANIFEST;

#if defined(OS_CHROMEOS)
  if (chromeos::system::InputDeviceSettings::Get()
          ->ForceKeyboardDrivenUINavigation()) {
    manifest_resource_id = IDR_GAIA_AUTH_KEYBOARD_MANIFEST;
  }
#endif

  component_loader->Add(manifest_resource_id,
                        base::FilePath(FILE_PATH_LITERAL("gaia_auth")));
}

void UnloadGaiaAuthExtension(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          context, GURL(chrome::kChromeUIChromeSigninURL));
  if (partition) {
    partition->ClearData(
        content::StoragePartition::REMOVE_DATA_MASK_ALL,
        content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
        GURL(),
        content::StoragePartition::OriginMatcherFunction(),
        base::Time(),
        base::Time::Max(),
        base::Bind(&base::DoNothing));
  }

  const char kGaiaAuthId[] = "mfffpogegjflfpflabcdkioaeobkgjik";
  GetComponentLoader(context)->Remove(kGaiaAuthId);
}

}  // namespace

namespace extensions {

GaiaAuthExtensionLoader::GaiaAuthExtensionLoader(BrowserContext* context)
    : browser_context_(context), load_count_(0) {}

GaiaAuthExtensionLoader::~GaiaAuthExtensionLoader() {
  DCHECK_EQ(0, load_count_);
}

void GaiaAuthExtensionLoader::LoadIfNeeded() {
  if (load_count_ == 0)
    LoadGaiaAuthExtension(browser_context_);
  ++load_count_;
}

void GaiaAuthExtensionLoader::UnloadIfNeeded() {
  --load_count_;
  if (load_count_ == 0)
    UnloadGaiaAuthExtension(browser_context_);
}

void GaiaAuthExtensionLoader::Shutdown() {
  if (load_count_ > 0) {
    UnloadGaiaAuthExtension(browser_context_);
    load_count_ = 0;
  }
}

// static
GaiaAuthExtensionLoader* GaiaAuthExtensionLoader::Get(BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>::Get(context);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>*
GaiaAuthExtensionLoader::GetFactoryInstance() {
  return g_factory.Pointer();
}

} // namespace extensions
