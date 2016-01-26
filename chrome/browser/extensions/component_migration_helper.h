// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_MIGRATION_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_MIGRATION_HELPER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;
class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;
class ExtensionSystem;

// For migrating existing extensions to component actions, and vice versa.  A
// previous enabled extension is used as a signal to add the corresponding
// component action to the visible area of the toolbar. This allows users who
// have already installed the extension to have their preference for a component
// action in the visible area of the toolbar respected, without enabling this
// for the entire user base.
//
// MIGRATION LOGIC
//
// When the feature is enabled (i.e. by experiment or flag), the client should
// call OnFeatureEnabled(action_id).  The extension action redesign MUST also be
// enabled.
//
//   - If the extension is enabled, it is unloaded with a reason of
//     MIGRATED_TO_COMPONENT, the component action shown, and a pref set
//     recording the migration.
//   - If pref is set the component action is shown.
//   - Otherwise, the component action is not shown.
//
// When the feature is disabled (for example, by starting with a flag off), the
// client should call OnFeatureDisabled(action_id).
//
//   - The pref is removed.
//   - If the extension action redesign is enabled, the associated component
//     action is removed.
//
// USAGE
// helper->Register("some-action-id", "some-extension-id");
// helper->Register("some-action-id", "other-extension-id");
// ...
// // When feature is enabled
// helper->OnFeatureEnabled("some-action-id");
// ...
// // When feature is disabled
// helper->OnFeatureDisabled("some-action-id");
//
// It is legal to register more than one extension per action but not vice
// versa.
class ComponentMigrationHelper : public ExtensionRegistryObserver {
 public:
  // Object that knows how to manage component actions in the toolbar model.
  class ComponentActionDelegate {
   public:
    // Adds or removes the component action labeled by |action_id| from the
    // toolbar model.  The caller will not add the same action twice.
    virtual void AddComponentAction(const std::string& action_id) = 0;
    virtual void RemoveComponentAction(const std::string& action_id) = 0;

    // Returns |true| if the toolbar model has an action for |action_id|.
    virtual bool HasComponentAction(const std::string& action_id) const = 0;
  };

  ComponentMigrationHelper(Profile* profile, ComponentActionDelegate* delegate);
  ~ComponentMigrationHelper() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Registers and unregisters a component action/extension pair.  A component
  // action may have more than one associated extension id, but not vice versa.
  void Register(const std::string& component_action_id,
                const ExtensionId& extension_id);
  void Unregister(const std::string& component_action_id,
                  const ExtensionId& extension_id);

  // Call when we should potentially add the component action and unload
  // the extension.  PREREQUISITE: The extension action redesign MUST be
  // enabled.
  void OnFeatureEnabled(const std::string& component_action_id);

  // Call when we should potentially remove the component action and re-enable
  // extension loading.
  void OnFeatureDisabled(const std::string& component_action_id);

  // Call when the user manually removes the component action from the toolbar.
  void OnActionRemoved(const std::string& component_action_id);

  // extensions::ExtensionRegistryObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;

 protected:
  // Protected for unit testing.
  void SetComponentActionPref(const std::string& component_action_id,
                              bool enabled);

  // A set of component action ids whose features are currently enabled.
  // Protected for unit testing.
  std::set<std::string> enabled_actions_;

 private:
  bool IsExtensionInstalledAndEnabled(const ExtensionId& extension_id) const;
  void UnloadExtension(const ExtensionId& extension_id);
  void RemoveComponentActionPref(const std::string& component_action_id);
  std::vector<std::string> GetExtensionIdsForActionId(
      const std::string& component_action_id) const;
  std::string GetActionIdForExtensionId(const ExtensionId& extension_id) const;

  Profile* const profile_;
  ComponentActionDelegate* const delegate_;
  // The ExtensionRegistry, PrefService, and ExtensionSystem, cached for
  // convenience.
  ExtensionRegistry* const extension_registry_;
  PrefService* const pref_service_;
  ExtensionSystem* const extension_system_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  // A list of pairs of component action ids and extension ids.
  std::vector<std::pair<std::string, ExtensionId>> migrated_actions_;

  DISALLOW_COPY_AND_ASSIGN(ComponentMigrationHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_MIGRATION_HELPER_H_
