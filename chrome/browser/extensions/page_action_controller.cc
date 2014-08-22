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
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

namespace extensions {

// Keeps track of the profiles for which we've sent UMA statistics.
base::LazyInstance<std::set<Profile*> > g_reported_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

PageActionController::PageActionController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      extension_action_observer_(this) {
  extension_action_observer_.Add(
      ExtensionActionAPI::Get(web_contents_->GetBrowserContext()));
}

PageActionController::~PageActionController() {
}

ExtensionAction* PageActionController::GetActionForExtension(
    const Extension* extension) {
  return ExtensionActionManager::Get(GetProfile())->GetPageAction(*extension);
}

ExtensionAction::ShowAction PageActionController::OnClicked(
    const Extension* extension) {
  ExtensionAction* page_action =
      ExtensionActionManager::Get(GetProfile())->GetPageAction(*extension);
  CHECK(page_action);

  int tab_id = SessionTabHelper::IdForTab(web_contents_);
  TabHelper::FromWebContents(web_contents_)->
      active_tab_permission_granter()->GrantIfRequested(extension);

  if (page_action->HasPopup(tab_id))
    return ExtensionAction::ACTION_SHOW_POPUP;

  ExtensionActionAPI::PageActionExecuted(
      web_contents_->GetBrowserContext(),
      *page_action,
      tab_id,
      web_contents_->GetLastCommittedURL().spec(),
      1 /* Button indication. We only ever pass left-click. */);

  return ExtensionAction::ACTION_NONE;
}

void PageActionController::OnNavigated() {
  // Clearing extension action values is handled in TabHelper, so nothing to
  // do here.
}

void PageActionController::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  if (web_contents == web_contents_ &&
      extension_action->action_type() == ActionInfo::TYPE_PAGE)
    LocationBarController::NotifyChange(web_contents_);
}

Profile* PageActionController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace extensions
