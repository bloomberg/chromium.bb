// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

ExtensionActionManager::ExtensionActionManager(Profile* profile) {
  CHECK_EQ(profile, profile->GetOriginalProfile())
      << "Don't instantiate this with an incognito profile.";
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ExtensionActionManager::~ExtensionActionManager() {
  // Don't assert that the ExtensionAction maps are empty because Extensions are
  // sometimes (only in tests?) not unloaded before the Profile is destroyed.
}

ExtensionActionManager* ExtensionActionManager::Get(Profile* profile) {
  return ExtensionSystem::Get(profile)->extension_action_manager();
}

void ExtensionActionManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension->page_action_info() &&
          !ContainsKey(page_actions_, extension->id())) {
        linked_ptr<ExtensionAction> page_action(
            new ExtensionAction(extension->id(),
                                Extension::ActionInfo::TYPE_PAGE,
                                *extension->page_action_info()));
        // The action box changes the meaning of the page action area, so we
        // need to convert page actions into browser actions.
        if (FeatureSwitch::script_badges()->IsEnabled())
          browser_actions_[extension->id()] = page_action;
        else
          page_actions_[extension->id()] = page_action;
      }
      if (extension->browser_action_info() &&
          !ContainsKey(browser_actions_, extension->id())) {
        linked_ptr<ExtensionAction> browser_action(
            new ExtensionAction(extension->id(),
                                Extension::ActionInfo::TYPE_BROWSER,
                                *extension->browser_action_info()));
        browser_actions_[extension->id()] = browser_action;
      }
      DCHECK(extension->script_badge_info());
      if (!ContainsKey(script_badges_, extension->id())) {
        linked_ptr<ExtensionAction> script_badge(
            new ExtensionAction(extension->id(),
                                Extension::ActionInfo::TYPE_SCRIPT_BADGE,
                                *extension->script_badge_info()));
        script_badges_[extension->id()] = script_badge;
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      page_actions_.erase(extension->id());
      browser_actions_.erase(extension->id());
      script_badges_.erase(extension->id());
      break;
    }
  }
}

namespace {

template<typename MapT>
typename MapT::mapped_type GetOrDefault(const MapT& map,
                                        const typename MapT::key_type& key) {
  typename MapT::const_iterator it = map.find(key);
  if (it == map.end())
    return typename MapT::mapped_type();
  return it->second;
}

}

ExtensionAction* ExtensionActionManager::GetPageAction(
    const extensions::Extension& extension) const {
  return GetOrDefault(page_actions_, extension.id()).get();
}

ExtensionAction* ExtensionActionManager::GetBrowserAction(
    const extensions::Extension& extension) const {
  return GetOrDefault(browser_actions_, extension.id()).get();
}

ExtensionAction* ExtensionActionManager::GetScriptBadge(
    const extensions::Extension& extension) const {
  return GetOrDefault(script_badges_, extension.id()).get();
}

}
