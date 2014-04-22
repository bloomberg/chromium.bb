// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace {

// BrowserContextKeyedServiceFactory for ExtensionActionManager.
class ExtensionActionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // BrowserContextKeyedServiceFactory implementation:
  static ExtensionActionManager* GetForProfile(Profile* profile) {
    return static_cast<ExtensionActionManager*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static ExtensionActionManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionActionManagerFactory>;

  ExtensionActionManagerFactory()
      : BrowserContextKeyedServiceFactory(
          "ExtensionActionManager",
          BrowserContextDependencyManager::GetInstance()) {
  }

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE {
    return new ExtensionActionManager(static_cast<Profile*>(profile));
  }

  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE {
    return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
  }
};

ExtensionActionManagerFactory*
ExtensionActionManagerFactory::GetInstance() {
  return Singleton<ExtensionActionManagerFactory>::get();
}

}  // namespace

ExtensionActionManager::ExtensionActionManager(Profile* profile)
    : profile_(profile), extension_registry_observer_(this) {
  CHECK_EQ(profile, profile->GetOriginalProfile())
      << "Don't instantiate this with an incognito profile.";
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

ExtensionActionManager::~ExtensionActionManager() {
  // Don't assert that the ExtensionAction maps are empty because Extensions are
  // sometimes (only in tests?) not unloaded before the Profile is destroyed.
}

ExtensionActionManager* ExtensionActionManager::Get(Profile* profile) {
  return ExtensionActionManagerFactory::GetForProfile(profile);
}

void ExtensionActionManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  page_actions_.erase(extension->id());
  browser_actions_.erase(extension->id());
  system_indicators_.erase(extension->id());
}

namespace {

// Returns map[extension_id] if that entry exists. Otherwise, if
// action_info!=NULL, creates an ExtensionAction from it, fills in the map, and
// returns that.  Otherwise (action_info==NULL), returns NULL.
ExtensionAction* GetOrCreateOrNull(
    std::map<std::string, linked_ptr<ExtensionAction> >* map,
    const std::string& extension_id,
    ActionInfo::Type action_type,
    const ActionInfo* action_info,
    Profile* profile) {
  std::map<std::string, linked_ptr<ExtensionAction> >::const_iterator it =
      map->find(extension_id);
  if (it != map->end())
    return it->second.get();
  if (!action_info)
    return NULL;

  // Only create action info for enabled extensions.
  // This avoids bugs where actions are recreated just after being removed
  // in response to OnExtensionUnloaded().
  ExtensionService* service =
      ExtensionSystem::Get(profile)->extension_service();
  if (!service->GetExtensionById(extension_id, false))
    return NULL;

  linked_ptr<ExtensionAction> action(new ExtensionAction(
      extension_id, action_type, *action_info));
  (*map)[extension_id] = action;
  return action.get();
}

}  // namespace

ExtensionAction* ExtensionActionManager::GetPageAction(
    const extensions::Extension& extension) const {
  return GetOrCreateOrNull(&page_actions_, extension.id(),
                           ActionInfo::TYPE_PAGE,
                           ActionInfo::GetPageActionInfo(&extension),
                           profile_);
}

ExtensionAction* ExtensionActionManager::GetBrowserAction(
    const extensions::Extension& extension) const {
  return GetOrCreateOrNull(&browser_actions_, extension.id(),
                           ActionInfo::TYPE_BROWSER,
                           ActionInfo::GetBrowserActionInfo(&extension),
                           profile_);
}

ExtensionAction* ExtensionActionManager::GetSystemIndicator(
    const extensions::Extension& extension) const {
  // If it does not already exist, create the SystemIndicatorManager for the
  // given profile.  This could return NULL if the system indicator area is
  // unavailable on the current system.  If so, return NULL to signal that
  // the system indicator area is unusable.
  if (!extensions::SystemIndicatorManagerFactory::GetForProfile(profile_))
    return NULL;

  return GetOrCreateOrNull(&system_indicators_, extension.id(),
                           ActionInfo::TYPE_SYSTEM_INDICATOR,
                           ActionInfo::GetSystemIndicatorInfo(&extension),
                           profile_);
}

}  // namespace extensions
