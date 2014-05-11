// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/page_action_controller.h"

#include <set>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

namespace extensions {

// Keeps track of the profiles for which we've sent UMA statistics.
base::LazyInstance<std::set<Profile*> > g_reported_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

PageActionController::PageActionController(content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

PageActionController::~PageActionController() {
}

ExtensionAction* PageActionController::GetActionForExtension(
    const Extension* extension) {
  return ExtensionActionManager::Get(GetProfile())->GetPageAction(*extension);
}

LocationBarController::Action PageActionController::OnClicked(
    const Extension* extension) {
  ExtensionAction* page_action =
      ExtensionActionManager::Get(GetProfile())->GetPageAction(*extension);
  CHECK(page_action);

  int tab_id = SessionID::IdForTab(web_contents_);
  TabHelper::FromWebContents(web_contents_)->
      active_tab_permission_granter()->GrantIfRequested(extension);

  if (page_action->HasPopup(tab_id))
    return LocationBarController::ACTION_SHOW_POPUP;

  ExtensionActionAPI::PageActionExecuted(
      web_contents_->GetBrowserContext(),
      *page_action,
      tab_id,
      web_contents_->GetLastCommittedURL().spec(),
      1 /* Button indication. We only ever pass left-click. */);

  return LocationBarController::ACTION_NONE;
}

void PageActionController::OnNavigated() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(web_contents_->GetBrowserContext())
          ->enabled_extensions();
  int tab_id = SessionID::IdForTab(web_contents_);
  size_t num_current_actions = 0u;
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    ExtensionAction* action = GetActionForExtension(*iter);
    if (action) {
      action->ClearAllValuesForTab(tab_id);
      ++num_current_actions;
    }
  }

  Profile* profile = GetProfile();
  // Report the number of page actions for this profile, if we haven't already.
  // TODO(rdevlin.cronin): This is wrong. Instead, it should record the number
  // of page actions displayed per page.
  if (!g_reported_for_profiles.Get().count(profile)) {
    UMA_HISTOGRAM_COUNTS_100("PageActionController.ExtensionsWithPageActions",
                             num_current_actions);
    g_reported_for_profiles.Get().insert(profile);
  }

  LocationBarController::NotifyChange(web_contents_);
}

Profile* PageActionController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace extensions
