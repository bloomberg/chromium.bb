// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_extension_system.h"

#include "extensions/common/extension_set.h"

namespace extensions {

MockExtensionSystem::MockExtensionSystem(content::BrowserContext* context)
    : browser_context_(context), event_router_(nullptr) {
}

MockExtensionSystem::~MockExtensionSystem() {
}

void MockExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
}

ExtensionService* MockExtensionSystem::extension_service() {
  return nullptr;
}

RuntimeData* MockExtensionSystem::runtime_data() {
  return nullptr;
}

ManagementPolicy* MockExtensionSystem::management_policy() {
  return nullptr;
}

SharedUserScriptMaster* MockExtensionSystem::shared_user_script_master() {
  return nullptr;
}

DeclarativeUserScriptManager*
MockExtensionSystem::declarative_user_script_manager() {
  return nullptr;
}

StateStore* MockExtensionSystem::state_store() {
  return nullptr;
}

StateStore* MockExtensionSystem::rules_store() {
  return nullptr;
}

InfoMap* MockExtensionSystem::info_map() {
  return nullptr;
}

LazyBackgroundTaskQueue* MockExtensionSystem::lazy_background_task_queue() {
  return nullptr;
}

EventRouter* MockExtensionSystem::event_router() {
  return event_router_;
}

ErrorConsole* MockExtensionSystem::error_console() {
  return nullptr;
}

InstallVerifier* MockExtensionSystem::install_verifier() {
  return nullptr;
}

QuotaService* MockExtensionSystem::quota_service() {
  return nullptr;
}

const OneShotEvent& MockExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* MockExtensionSystem::content_verifier() {
  return nullptr;
}

scoped_ptr<ExtensionSet> MockExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return scoped_ptr<ExtensionSet>();
}

}  // namespace extensions
