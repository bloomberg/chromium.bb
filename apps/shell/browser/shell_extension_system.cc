// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_extension_system.h"

#include <string>

#include "apps/shell/browser/shell_app_runtime_api.h"
#include "base/files/file_path.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/file_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

ShellExtensionSystem::ShellExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ShellExtensionSystem::~ShellExtensionSystem() {
}

bool ShellExtensionSystem::LoadAndLaunchApp(const base::FilePath& app_dir) {
  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      app_dir, Manifest::COMMAND_LINE, Extension::NO_FLAGS, &load_error);
  if (!extension) {
    LOG(ERROR) << "Loading extension at " << app_dir.value()
        << " failed with: " << load_error;
    return false;
  }
  app_id_ = extension->id();

  // TODO(jamescook): We may want to do some of these things here:
  // * Create a PermissionsUpdater.
  // * Call PermissionsUpdater::GrantActivePermissions().
  // * Call ExtensionService::SatisfyImports().
  // * Call ExtensionPrefs::OnExtensionInstalled().
  // * Send NOTIFICATION_EXTENSION_INSTALLED.

  ExtensionRegistry::Get(browser_context_)->AddEnabled(extension);

  RegisterExtensionWithRequestContexts(extension);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<const Extension>(extension));

  // Inform the rest of the extensions system to start.
  ready_.Signal();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSIONS_READY,
      content::Source<BrowserContext>(browser_context_),
      content::NotificationService::NoDetails());

  // Send the onLaunched event.
  ShellAppRuntimeAPI::DispatchOnLaunchedEvent(event_router_.get(),
                                              extension.get());

  return true;
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
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 make_scoped_refptr(extension), base::Time::Now(),
                 false, false));
}

void ShellExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
}

const OneShotEvent& ShellExtensionSystem::ready() const {
  return ready_;
}

}  // namespace extensions
