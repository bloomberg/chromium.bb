// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/page_action_controller.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"

namespace extensions {

// Keeps track of the profiles for which we've sent UMA statistics.
base::LazyInstance<std::set<Profile*> > g_reported_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

PageActionController::PageActionController(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PageActionController::~PageActionController() {}

std::vector<ExtensionAction*> PageActionController::GetCurrentActions() const {
  ExtensionService* service = GetExtensionService();
  if (!service)
    return std::vector<ExtensionAction*>();

  // Accumulate the list of all page actions to display.
  std::vector<ExtensionAction*> current_actions;

  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(profile());

  for (ExtensionSet::const_iterator i = service->extensions()->begin();
       i != service->extensions()->end(); ++i) {
    ExtensionAction* action =
        extension_action_manager->GetPageAction(*i->get());
    if (action)
      current_actions.push_back(action);
  }

  if (!g_reported_for_profiles.Get().count(profile())) {
    UMA_HISTOGRAM_COUNTS_100("PageActionController.ExtensionsWithPageActions",
                             current_actions.size());
    g_reported_for_profiles.Get().insert(profile());
  }

  return current_actions;
}

LocationBarController::Action PageActionController::OnClicked(
    const std::string& extension_id, int mouse_button) {
  ExtensionService* service = GetExtensionService();
  if (!service)
    return ACTION_NONE;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  CHECK(extension);
  ExtensionAction* page_action =
      ExtensionActionManager::Get(profile())->GetPageAction(*extension);
  CHECK(page_action);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents());

  extensions::TabHelper::FromWebContents(web_contents())->
      active_tab_permission_granter()->GrantIfRequested(extension);

  switch (mouse_button) {
    case 1:  // left
    case 2:  // middle
      if (page_action->HasPopup(tab_id))
        return ACTION_SHOW_POPUP;

      ExtensionActionAPI::PageActionExecuted(
          profile(), *page_action, tab_id,
          web_contents()->GetURL().spec(), mouse_button);
      return ACTION_NONE;

    case 3:  // right
      return extension->ShowConfigureContextMenus() ?
          ACTION_SHOW_CONTEXT_MENU : ACTION_NONE;
  }

  return ACTION_NONE;
}

void PageActionController::NotifyChange() {
  web_contents()->NotifyNavigationStateChanged(
      content::INVALIDATE_TYPE_PAGE_ACTIONS);
}

void PageActionController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  const std::vector<ExtensionAction*> current_actions = GetCurrentActions();

  if (current_actions.empty())
    return;

  for (size_t i = 0; i < current_actions.size(); ++i) {
    current_actions[i]->ClearAllValuesForTab(
        SessionID::IdForTab(web_contents()));
  }

  NotifyChange();
}

Profile* PageActionController::profile() const {
  content::WebContents* web_contents = this->web_contents();
  if (web_contents)
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());

  return NULL;
}

ExtensionService* PageActionController::GetExtensionService() const {
  Profile* profile = this->profile();
  if (profile)
    return ExtensionSystem::Get(profile)->extension_service();

  return NULL;
}

}  // namespace extensions
