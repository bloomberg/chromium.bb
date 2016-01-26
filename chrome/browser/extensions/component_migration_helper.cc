// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_migration_helper.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"

namespace extensions {

ComponentMigrationHelper::ComponentMigrationHelper(
    Profile* profile,
    ComponentActionDelegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      extension_registry_(ExtensionRegistry::Get(profile)),
      pref_service_(profile->GetPrefs()),
      extension_system_(ExtensionSystem::Get(profile)),
      extension_registry_observer_(this) {
  DCHECK(delegate_);
  extension_registry_observer_.Add(extension_registry_);
}

ComponentMigrationHelper::~ComponentMigrationHelper() {}

// static
void ComponentMigrationHelper::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      ::prefs::kToolbarMigratedComponentActionStatus);
}

void ComponentMigrationHelper::Register(const std::string& component_action_id,
                                        const ExtensionId& extension_id) {
  DCHECK(GetActionIdForExtensionId(extension_id).empty());
  migrated_actions_.push_back(
      std::make_pair(component_action_id, extension_id));
}

void ComponentMigrationHelper::Unregister(
    const std::string& component_action_id,
    const ExtensionId& extension_id) {
  for (auto it = migrated_actions_.begin(); it != migrated_actions_.end();
       it++) {
    if (it->first == component_action_id && it->second == extension_id) {
      migrated_actions_.erase(it);
      return;
    }
  }
}

void ComponentMigrationHelper::OnFeatureEnabled(
    const std::string& component_action_id) {
  DCHECK(FeatureSwitch::extension_action_redesign()->IsEnabled());
  std::vector<ExtensionId> extension_ids =
      GetExtensionIdsForActionId(component_action_id);
  DCHECK(!extension_ids.empty());

  enabled_actions_.insert(component_action_id);
  // Unload any extensions that we are migrating from.
  bool extension_was_installed = false;
  for (const auto& id : extension_ids) {
    if (IsExtensionInstalledAndEnabled(id)) {
      extension_was_installed = true;
      UnloadExtension(id);
    }
  }

  // Read the pref to determine component action status.  If not set, set to
  // true if we unloaded an extension.
  const base::DictionaryValue* migration_pref = pref_service_->GetDictionary(
      ::prefs::kToolbarMigratedComponentActionStatus);
  bool has_component_action_pref = migration_pref->HasKey(component_action_id);
  bool component_action_pref = false;
  if (has_component_action_pref) {
    bool success =
        migration_pref->GetBoolean(component_action_id, &component_action_pref);
    DCHECK(success);  // Shouldn't fail, but can in case of pref corruption.
  }

  if (!has_component_action_pref && extension_was_installed) {
    SetComponentActionPref(component_action_id, true);
    component_action_pref = true;
  }

  if (component_action_pref &&
      !delegate_->HasComponentAction(component_action_id)) {
    delegate_->AddComponentAction(component_action_id);
  }
}

void ComponentMigrationHelper::OnFeatureDisabled(
    const std::string& component_action_id) {
  std::vector<ExtensionId> extension_ids =
      GetExtensionIdsForActionId(component_action_id);
  DCHECK(!extension_ids.empty());

  enabled_actions_.erase(component_action_id);
  RemoveComponentActionPref(component_action_id);

  if (FeatureSwitch::extension_action_redesign()->IsEnabled() &&
      delegate_->HasComponentAction(component_action_id))
    delegate_->RemoveComponentAction(component_action_id);
}

void ComponentMigrationHelper::OnActionRemoved(
    const std::string& component_action_id) {
  // Record preference for the future.
  SetComponentActionPref(component_action_id, false);

  // Remove the action.
  if (delegate_->HasComponentAction(component_action_id))
    delegate_->RemoveComponentAction(component_action_id);
}

void ComponentMigrationHelper::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const ExtensionId& extension_id = extension->id();
  const std::string& component_action_id =
      GetActionIdForExtensionId(extension_id);
  if (component_action_id.empty())
    return;
  if (ContainsKey(enabled_actions_, component_action_id)) {
    UnloadExtension(extension_id);
    SetComponentActionPref(component_action_id, true);

    if (!delegate_->HasComponentAction(component_action_id))
      delegate_->AddComponentAction(component_action_id);
  }
}

void ComponentMigrationHelper::SetComponentActionPref(
    const std::string& component_action_id,
    bool enabled) {
  DictionaryPrefUpdate update(pref_service_,
                              ::prefs::kToolbarMigratedComponentActionStatus);
  update->SetBoolean(component_action_id, enabled);
}

bool ComponentMigrationHelper::IsExtensionInstalledAndEnabled(
    const ExtensionId& extension_id) const {
  return extension_registry_->enabled_extensions().Contains(extension_id) ||
         extension_registry_->terminated_extensions().Contains(extension_id);
}

void ComponentMigrationHelper::UnloadExtension(
    const ExtensionId& extension_id) {
  extension_system_->extension_service()->UnloadExtension(
      extension_id, UnloadedExtensionInfo::REASON_MIGRATED_TO_COMPONENT);
}

void ComponentMigrationHelper::RemoveComponentActionPref(
    const std::string& component_action_id) {
  DictionaryPrefUpdate update(pref_service_,
                              ::prefs::kToolbarMigratedComponentActionStatus);
  update->Remove(component_action_id, nullptr);
}

std::vector<std::string> ComponentMigrationHelper::GetExtensionIdsForActionId(
    const std::string& component_action_id) const {
  std::vector<ExtensionId> extension_ids;
  for (const auto& i : migrated_actions_) {
    if (i.first == component_action_id)
      extension_ids.push_back(i.second);
  }
  return extension_ids;
}

std::string ComponentMigrationHelper::GetActionIdForExtensionId(
    const ExtensionId& extension_id) const {
  for (const auto& i : migrated_actions_) {
    if (i.second == extension_id)
      return i.first;
  }
  return "";
}

}  // namespace extensions
