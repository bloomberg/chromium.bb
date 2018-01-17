// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extension_system.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/value_store/value_store_factory_impl.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

CastExtensionSystem::CastExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context),
      store_factory_(new ValueStoreFactoryImpl(browser_context->GetPath())),
      weak_factory_(this) {}

CastExtensionSystem::~CastExtensionSystem() {}

const Extension* CastExtensionSystem::LoadExtension(
    const base::FilePath& extension_dir) {
  // cast_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  CHECK(base::DirectoryExists(extension_dir)) << extension_dir.AsUTF8Unsafe();
  int load_flags = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, Manifest::COMMAND_LINE, load_flags, &load_error);
  if (!extension.get()) {
    LOG(ERROR) << "Loading extension at " << extension_dir.value()
               << " failed with: " << load_error;
    return nullptr;
  }

  // Log warnings.
  if (extension->install_warnings().size()) {
    LOG(WARNING) << "Warnings loading extension at " << extension_dir.value()
                 << ":";
    for (const auto& warning : extension->install_warnings())
      LOG(WARNING) << warning.message;
  }

  // TODO(b/70902491): We may want to do some of these things here:
  // * Create a PermissionsUpdater.
  // * Call PermissionsUpdater::GrantActivePermissions().
  // * Call ExtensionService::SatisfyImports().
  // * Call ExtensionPrefs::OnExtensionInstalled().
  // * Call ExtensionRegistryObserver::OnExtensionWillbeInstalled().

  ExtensionRegistry::Get(browser_context_)->AddEnabled(extension.get());

  RegisterExtensionWithRequestContexts(
      extension.get(),
      base::Bind(&CastExtensionSystem::OnExtensionRegisteredWithRequestContexts,
                 weak_factory_.GetWeakPtr(), extension));

  RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
      ->OnExtensionLoaded(*extension);

  ExtensionRegistry::Get(browser_context_)->TriggerOnLoaded(extension.get());

  return extension.get();
}

const Extension* CastExtensionSystem::LoadApp(const base::FilePath& app_dir) {
  return LoadExtension(app_dir);
}

void CastExtensionSystem::Init() {
  // Inform the rest of the extensions system to start.
  ready_.Signal();
  content::NotificationService::current()->Notify(
      NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::NotificationService::NoDetails());
}

void CastExtensionSystem::LaunchApp(const ExtensionId& extension_id) {
  // Send the onLaunched event.
  DCHECK(ExtensionRegistry::Get(browser_context_)
             ->enabled_extensions()
             .Contains(extension_id));
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  AppRuntimeEventRouter::DispatchOnLaunchedEvent(browser_context_, extension,
                                                 SOURCE_UNTRACKED, nullptr);
}

void CastExtensionSystem::Shutdown() {}

void CastExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  service_worker_manager_ =
      std::make_unique<ServiceWorkerManager>(browser_context_);
  runtime_data_ =
      std::make_unique<RuntimeData>(ExtensionRegistry::Get(browser_context_));
  quota_service_ = std::make_unique<QuotaService>();
  app_sorting_ = std::make_unique<NullAppSorting>();

  RendererStartupHelperFactory::GetForBrowserContext(browser_context_);
}

void CastExtensionSystem::InitForIncognitoProfile() {
  NOTREACHED();
}

ExtensionService* CastExtensionSystem::extension_service() {
  return nullptr;
}

RuntimeData* CastExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* CastExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* CastExtensionSystem::service_worker_manager() {
  return service_worker_manager_.get();
}

SharedUserScriptMaster* CastExtensionSystem::shared_user_script_master() {
  return nullptr;
}

StateStore* CastExtensionSystem::state_store() {
  return nullptr;
}

StateStore* CastExtensionSystem::rules_store() {
  return nullptr;
}

scoped_refptr<ValueStoreFactory> CastExtensionSystem::store_factory() {
  return store_factory_;
}

InfoMap* CastExtensionSystem::info_map() {
  if (!info_map_.get())
    info_map_ = base::MakeRefCounted<InfoMap>();
  return info_map_.get();
}

QuotaService* CastExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* CastExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

void CastExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension,
    const base::Closure& callback) {
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 base::RetainedRef(extension), base::Time::Now(), false, false),
      callback);
}

void CastExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionReason reason) {}

const OneShotEvent& CastExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* CastExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> CastExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return std::make_unique<ExtensionSet>();
}

void CastExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

void CastExtensionSystem::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<Extension> extension) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  registry->AddReady(extension);
  registry->TriggerOnReady(extension.get());
}

}  // namespace extensions
