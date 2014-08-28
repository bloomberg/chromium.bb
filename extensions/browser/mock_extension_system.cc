// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_extension_system.h"

#include "extensions/common/extension_set.h"

namespace extensions {

//
// MockExtensionSystem
//

MockExtensionSystem::MockExtensionSystem(content::BrowserContext* context)
    : browser_context_(context) {
}

MockExtensionSystem::~MockExtensionSystem() {
}

void MockExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
}

ExtensionService* MockExtensionSystem::extension_service() {
  return NULL;
}

RuntimeData* MockExtensionSystem::runtime_data() {
  return NULL;
}

ManagementPolicy* MockExtensionSystem::management_policy() {
  return NULL;
}

SharedUserScriptMaster* MockExtensionSystem::shared_user_script_master() {
  return NULL;
}

ProcessManager* MockExtensionSystem::process_manager() {
  return NULL;
}

StateStore* MockExtensionSystem::state_store() {
  return NULL;
}

StateStore* MockExtensionSystem::rules_store() {
  return NULL;
}

InfoMap* MockExtensionSystem::info_map() {
  return NULL;
}

LazyBackgroundTaskQueue* MockExtensionSystem::lazy_background_task_queue() {
  return NULL;
}

EventRouter* MockExtensionSystem::event_router() {
  return NULL;
}

WarningService* MockExtensionSystem::warning_service() {
  return NULL;
}

Blacklist* MockExtensionSystem::blacklist() {
  return NULL;
}

ErrorConsole* MockExtensionSystem::error_console() {
  return NULL;
}

InstallVerifier* MockExtensionSystem::install_verifier() {
  return NULL;
}

QuotaService* MockExtensionSystem::quota_service() {
  return NULL;
}

const OneShotEvent& MockExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* MockExtensionSystem::content_verifier() {
  return NULL;
}

DeclarativeUserScriptMaster*
      MockExtensionSystem::GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) {
  return NULL;
}

scoped_ptr<ExtensionSet> MockExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return scoped_ptr<ExtensionSet>();
}

}  // namespace extensions
