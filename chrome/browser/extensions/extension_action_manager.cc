// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

ExtensionActionManager::ExtensionActionManager(Profile* profile)
    : profile_(profile) {
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

// This will be unnecessary when the accessors within Extension go away.
void ExtensionActionManager::Shutdown() {
  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  if (!extension_service) {
    // Some tests fire the EXTENSION_LOADED notification without using
    // an ExtensionService.  For them, don't bother nulling out the
    // extension fields on shutdown.
    return;
  }

  for (ExtIdToActionMap::const_iterator it = page_actions_.begin();
       it != page_actions_.end(); ++it) {
    const std::string& extension_id = it->second->extension_id();
    const Extension* extension =
        extension_service->GetExtensionById(extension_id,
                                            /*include_disabled=*/true);
    if (extension) {
      // If the Extension is still alive, make sure it doesn't have a
      // dangling pointer to the destroyed ExtensionAction.
      extension->page_action_ = NULL;
    }
  }
  for (ExtIdToActionMap::const_iterator it = browser_actions_.begin();
       it != browser_actions_.end(); ++it) {
    const std::string& extension_id = it->second->extension_id();
    const Extension* extension =
        extension_service->GetExtensionById(extension_id,
                                            /*include_disabled=*/true);
    if (extension) {
      // If the Extension is still alive, make sure it doesn't have a
      // dangling pointer to the destroyed ExtensionAction.
      extension->browser_action_ = NULL;
    }
  }
  for (ExtIdToActionMap::const_iterator it = script_badges_.begin();
       it != script_badges_.end(); ++it) {
    const std::string& extension_id = it->second->extension_id();
    const Extension* extension =
        extension_service->GetExtensionById(extension_id,
                                            /*include_disabled=*/true);
    if (extension) {
      // If the Extension is still alive, make sure it doesn't have a
      // dangling pointer to the destroyed ExtensionAction.
      extension->script_badge_ = NULL;
    }
  }
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
        if (switch_utils::AreScriptBadgesEnabled()) {
          browser_actions_[extension->id()] = page_action;
          extension->browser_action_ = page_action.get();
        } else {
          page_actions_[extension->id()] = page_action;
          extension->page_action_ = page_action.get();
        }
      }
      if (extension->browser_action_info() &&
          !ContainsKey(browser_actions_, extension->id())) {
        linked_ptr<ExtensionAction> browser_action(
            new ExtensionAction(extension->id(),
                                Extension::ActionInfo::TYPE_BROWSER,
                                *extension->browser_action_info()));
        browser_actions_[extension->id()] = browser_action;
        extension->browser_action_ = browser_action.get();
      }
      DCHECK(extension->script_badge_info());
      if (!ContainsKey(script_badges_, extension->id())) {
        linked_ptr<ExtensionAction> script_badge(
            new ExtensionAction(extension->id(),
                                Extension::ActionInfo::TYPE_SCRIPT_BADGE,
                                *extension->script_badge_info()));
        script_badges_[extension->id()] = script_badge;
        extension->script_badge_ = script_badge.get();
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (ContainsKey(page_actions_, extension->id())) {
        extension->page_action_ = NULL;
        page_actions_.erase(extension->id());
      }
      if (ContainsKey(browser_actions_, extension->id())) {
        extension->browser_action_ = NULL;
        browser_actions_.erase(extension->id());
      }
      if (ContainsKey(script_badges_, extension->id())) {
        extension->script_badge_ = NULL;
        script_badges_.erase(extension->id());
      }
      break;
    }
  }
}

}
