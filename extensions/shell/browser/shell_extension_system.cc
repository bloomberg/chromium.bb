// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_system.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/file_util.h"
#include "extensions/shell/browser/api/shell/shell_api.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

ShellExtensionSystem::ShellExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ShellExtensionSystem::~ShellExtensionSystem() {
}

bool ShellExtensionSystem::LoadApp(const base::FilePath& app_dir) {
  // app_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  CHECK(base::DirectoryExists(app_dir)) << app_dir.AsUTF8Unsafe();
  int load_flags = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::string load_error;
  extension_ = file_util::LoadExtension(
      app_dir, Manifest::COMMAND_LINE, load_flags, &load_error);
  if (!extension_) {
    LOG(ERROR) << "Loading extension at " << app_dir.value()
               << " failed with: " << load_error;
    return false;
  }
  app_id_ = extension_->id();

  // TODO(jamescook): We may want to do some of these things here:
  // * Create a PermissionsUpdater.
  // * Call PermissionsUpdater::GrantActivePermissions().
  // * Call ExtensionService::SatisfyImports().
  // * Call ExtensionPrefs::OnExtensionInstalled().
  // * Send NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED.

  ExtensionRegistry::Get(browser_context_)->AddEnabled(extension_);

  RegisterExtensionWithRequestContexts(extension_);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<const Extension>(extension_));

  // Inform the rest of the extensions system to start.
  ready_.Signal();
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::NotificationService::NoDetails());
  return true;
}

void ShellExtensionSystem::LaunchApp() {
  // Send the onLaunched event.
  DCHECK(extension_.get());
  AppRuntimeEventRouter::DispatchOnLaunchedEvent(browser_context_,
                                                 extension_.get());
}

void ShellExtensionSystem::Shutdown() {
}

void ShellExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  runtime_data_.reset(
      new RuntimeData(ExtensionRegistry::Get(browser_context_)));
  lazy_background_task_queue_.reset(
      new LazyBackgroundTaskQueue(browser_context_));
  event_router_.reset(
      new EventRouter(browser_context_, ExtensionPrefs::Get(browser_context_)));
  process_manager_.reset(ProcessManager::Create(browser_context_));
  quota_service_.reset(new QuotaService);
}

ExtensionService* ShellExtensionSystem::extension_service() {
  return NULL;
}

RuntimeData* ShellExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* ShellExtensionSystem::management_policy() {
  return NULL;
}

UserScriptMaster* ShellExtensionSystem::user_script_master() {
  return NULL;
}

ProcessManager* ShellExtensionSystem::process_manager() {
  return process_manager_.get();
}

StateStore* ShellExtensionSystem::state_store() {
  return NULL;
}

StateStore* ShellExtensionSystem::rules_store() {
  return NULL;
}

InfoMap* ShellExtensionSystem::info_map() {
  if (!info_map_.get())
    info_map_ = new InfoMap;
  return info_map_;
}

LazyBackgroundTaskQueue* ShellExtensionSystem::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

EventRouter* ShellExtensionSystem::event_router() {
  return event_router_.get();
}

ExtensionWarningService* ShellExtensionSystem::warning_service() {
  return NULL;
}

Blacklist* ShellExtensionSystem::blacklist() {
  return NULL;
}

ErrorConsole* ShellExtensionSystem::error_console() {
  return NULL;
}

InstallVerifier* ShellExtensionSystem::install_verifier() {
  return NULL;
}

QuotaService* ShellExtensionSystem::quota_service() {
  return quota_service_.get();
}

void ShellExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&InfoMap::AddExtension,
                                     info_map(),
                                     make_scoped_refptr(extension),
                                     base::Time::Now(),
                                     false,
                                     false));
}

void ShellExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
}

const OneShotEvent& ShellExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* ShellExtensionSystem::content_verifier() {
  return NULL;
}

scoped_ptr<ExtensionSet> ShellExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  scoped_ptr<ExtensionSet> empty(new ExtensionSet());
  return empty.PassAs<ExtensionSet>();
}

}  // namespace extensions
