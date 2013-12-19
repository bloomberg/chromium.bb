// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_extension_system.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"

using content::BrowserContext;

namespace extensions {

ShellExtensionSystem::ShellExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ShellExtensionSystem::~ShellExtensionSystem() {
}

void ShellExtensionSystem::Shutdown() {
}

void ShellExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  lazy_background_task_queue_.reset(
      new LazyBackgroundTaskQueue(browser_context_));
  event_router_.reset(
      new EventRouter(browser_context_, ExtensionPrefs::Get(browser_context_)));
  process_manager_.reset(ProcessManager::Create(browser_context_));
}

ExtensionService* ShellExtensionSystem::extension_service() {
  // This class only has an ExtensionServiceInterface.
  return NULL;
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
  return NULL;
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

void ShellExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
}

void ShellExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
}

const OneShotEvent& ShellExtensionSystem::ready() const {
  return ready_;
}

}  // namespace extensions
