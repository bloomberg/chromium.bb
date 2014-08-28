// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

namespace extensions {

namespace {

// BrowserContextKeyedServiceFactory for ExtensionActionManager.
class ExtensionActionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // BrowserContextKeyedServiceFactory implementation:
  static ExtensionActionManager* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<ExtensionActionManager*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
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

ExtensionActionManager* ExtensionActionManager::Get(
    content::BrowserContext* context) {
  return ExtensionActionManagerFactory::GetForBrowserContext(context);
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

// Loads resources missing from |action| (i.e. title, icons) from the "icons"
// key of |extension|'s manifest.
void PopulateMissingValues(const Extension& extension,
                           ExtensionAction* action) {
  // If the title is missing from |action|, set it to |extension|'s name.
  if (action->GetTitle(ExtensionAction::kDefaultTabId).empty())
    action->SetTitle(ExtensionAction::kDefaultTabId, extension.name());

  scoped_ptr<ExtensionIconSet> default_icon(new ExtensionIconSet());
  if (action->default_icon())
    *default_icon = *action->default_icon();

  const ExtensionIconSet& extension_icons = IconsInfo::GetIcons(&extension);
  std::string largest_icon = extension_icons.Get(
      extension_misc::EXTENSION_ICON_GIGANTOR,
      ExtensionIconSet::MATCH_SMALLER);

  if (!largest_icon.empty()) {
    int largest_icon_size = extension_icons.GetIconSizeFromPath(largest_icon);
    // Replace any missing extension action icons with the largest icon
    // retrieved from |extension|'s manifest so long as the largest icon is
    // larger than the current key.
    for (int i = extension_misc::kNumExtensionActionIconSizes - 1;
         i >= 0; --i) {
      int size = extension_misc::kExtensionActionIconSizes[i].size;
      if (default_icon->Get(size, ExtensionIconSet::MATCH_BIGGER).empty()
          && largest_icon_size > size) {
        default_icon->Add(size, largest_icon);
        break;
      }
    }
    action->set_default_icon(default_icon.Pass());
  }
}

// Returns map[extension_id] if that entry exists. Otherwise, if
// action_info!=NULL, creates an ExtensionAction from it, fills in the map, and
// returns that.  Otherwise (action_info==NULL), returns NULL.
ExtensionAction* GetOrCreateOrNull(
    std::map<std::string, linked_ptr<ExtensionAction> >* map,
    const Extension& extension,
    ActionInfo::Type action_type,
    const ActionInfo* action_info,
    Profile* profile) {
  std::map<std::string, linked_ptr<ExtensionAction> >::const_iterator it =
      map->find(extension.id());
  if (it != map->end())
    return it->second.get();
  if (!action_info)
    return NULL;

  // Only create action info for enabled extensions.
  // This avoids bugs where actions are recreated just after being removed
  // in response to OnExtensionUnloaded().
  if (!ExtensionRegistry::Get(profile)
      ->enabled_extensions().Contains(extension.id())) {
    return NULL;
  }

  linked_ptr<ExtensionAction> action(new ExtensionAction(
      extension.id(), action_type, *action_info));
  (*map)[extension.id()] = action;
  PopulateMissingValues(extension, action.get());
  return action.get();
}

}  // namespace

ExtensionAction* ExtensionActionManager::GetPageAction(
    const Extension& extension) const {
  return GetOrCreateOrNull(&page_actions_, extension,
                           ActionInfo::TYPE_PAGE,
                           ActionInfo::GetPageActionInfo(&extension),
                           profile_);
}

ExtensionAction* ExtensionActionManager::GetBrowserAction(
    const Extension& extension) const {
  return GetOrCreateOrNull(&browser_actions_, extension,
                           ActionInfo::TYPE_BROWSER,
                           ActionInfo::GetBrowserActionInfo(&extension),
                           profile_);
}

scoped_ptr<ExtensionAction> ExtensionActionManager::GetBestFitAction(
    const Extension& extension,
    ActionInfo::Type type) const {
  const ActionInfo* info = ActionInfo::GetBrowserActionInfo(&extension);
  if (!info)
    info = ActionInfo::GetPageActionInfo(&extension);

  // Create a new ExtensionAction of |type| with |extension|'s ActionInfo.
  // If no ActionInfo exists for |extension|, create and return a new action
  // with a blank ActionInfo.
  // Populate any missing values from |extension|'s manifest.
  scoped_ptr<ExtensionAction> new_action(new ExtensionAction(
      extension.id(), type, info ? *info : ActionInfo()));
  PopulateMissingValues(extension, new_action.get());
  return new_action.Pass();
}

ExtensionAction* ExtensionActionManager::GetSystemIndicator(
    const Extension& extension) const {
  // If it does not already exist, create the SystemIndicatorManager for the
  // given profile.  This could return NULL if the system indicator area is
  // unavailable on the current system.  If so, return NULL to signal that
  // the system indicator area is unusable.
  if (!SystemIndicatorManagerFactory::GetForProfile(profile_))
    return NULL;

  return GetOrCreateOrNull(&system_indicators_, extension,
                           ActionInfo::TYPE_SYSTEM_INDICATOR,
                           ActionInfo::GetSystemIndicatorInfo(&extension),
                           profile_);
}

ExtensionAction* ExtensionActionManager::GetExtensionAction(
    const Extension& extension) const {
  ExtensionAction* action = GetBrowserAction(extension);
  return action ? action : GetPageAction(extension);
}

}  // namespace extensions
