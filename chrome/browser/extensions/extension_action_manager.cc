// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace {

// ProfileKeyedServiceFactory for ExtensionActionManager.
class ExtensionActionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // ProfileKeyedServiceFactory implementation:
  static ExtensionActionManager* GetForProfile(Profile* profile) {
    return static_cast<ExtensionActionManager*>(
        GetInstance()->GetServiceForProfile(profile, true));
  }

  static ExtensionActionManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionActionManagerFactory>;

  ExtensionActionManagerFactory()
      : ProfileKeyedServiceFactory("ExtensionActionManager",
                                   ProfileDependencyManager::GetInstance()) {
  }

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE {
    return new ExtensionActionManager(profile);
  }

  virtual bool ServiceRedirectedInIncognito() const OVERRIDE {
    return true;
  }
};

ExtensionActionManagerFactory*
ExtensionActionManagerFactory::GetInstance() {
  return Singleton<ExtensionActionManagerFactory>::get();
}

}  // namespace

ExtensionActionManager::ExtensionActionManager(Profile* profile)
    : profile_(profile) {
  CHECK_EQ(profile, profile->GetOriginalProfile())
      << "Don't instantiate this with an incognito profile.";
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ExtensionActionManager::~ExtensionActionManager() {
  // Don't assert that the ExtensionAction maps are empty because Extensions are
  // sometimes (only in tests?) not unloaded before the Profile is destroyed.
}

ExtensionActionManager* ExtensionActionManager::Get(Profile* profile) {
  return ExtensionActionManagerFactory::GetForProfile(profile);
}

void ExtensionActionManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      page_actions_.erase(extension->id());
      browser_actions_.erase(extension->id());
      script_badges_.erase(extension->id());
      system_indicators_.erase(extension->id());
      break;
    }
  }
}

namespace {

// Returns map[extension_id] if that entry exists. Otherwise, if
// action_info!=NULL, creates an ExtensionAction from it, fills in the map, and
// returns that.  Otherwise (action_info==NULL), returns NULL.
ExtensionAction* GetOrCreateOrNull(
    std::map<std::string, linked_ptr<ExtensionAction> >* map,
    const std::string& extension_id,
    ActionInfo::Type action_type,
    const ActionInfo* action_info) {
  std::map<std::string, linked_ptr<ExtensionAction> >::const_iterator it =
      map->find(extension_id);
  if (it != map->end())
    return it->second.get();
  if (!action_info)
    return NULL;
  linked_ptr<ExtensionAction> action(new ExtensionAction(
      extension_id, action_type, *action_info));
  (*map)[extension_id] = action;
  return action.get();
}

}  // namespace

ExtensionAction* ExtensionActionManager::GetPageAction(
    const extensions::Extension& extension) const {
  // The action box changes the meaning of the page action area, so we
  // need to convert page actions into browser actions.
  if (FeatureSwitch::script_badges()->IsEnabled())
    return NULL;
  return GetOrCreateOrNull(&page_actions_, extension.id(),
                           ActionInfo::TYPE_PAGE,
                           extension.page_action_info());
}

ExtensionAction* ExtensionActionManager::GetBrowserAction(
    const extensions::Extension& extension) const {
  const ActionInfo* action_info = extension.browser_action_info();
  ActionInfo::Type action_type = ActionInfo::TYPE_BROWSER;
  if (FeatureSwitch::script_badges()->IsEnabled() &&
      extension.page_action_info()) {
    // The action box changes the meaning of the page action area, so we
    // need to convert page actions into browser actions.
    action_info = extension.page_action_info();
    action_type = ActionInfo::TYPE_PAGE;
  }
  return GetOrCreateOrNull(&browser_actions_, extension.id(),
                           action_type, action_info);
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
                           extension.system_indicator_info());
}

ExtensionAction* ExtensionActionManager::GetScriptBadge(
    const extensions::Extension& extension) const {
  return GetOrCreateOrNull(&script_badges_, extension.id(),
                           ActionInfo::TYPE_SCRIPT_BADGE,
                           ActionInfo::GetScriptBadgeInfo(&extension));
}

}  // namespace extensions
