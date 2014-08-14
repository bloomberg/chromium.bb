// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/location_bar_controller.h"

#include "base/logging.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/page_action_controller.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

LocationBarController::LocationBarController(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      active_script_controller_(new ActiveScriptController(web_contents_)),
      page_action_controller_(new PageActionController(web_contents_)),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(
      ExtensionRegistry::Get(web_contents_->GetBrowserContext()));
}

LocationBarController::~LocationBarController() {
}

std::vector<ExtensionAction*> LocationBarController::GetCurrentActions() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(web_contents_->GetBrowserContext())
          ->enabled_extensions();
  std::vector<ExtensionAction*> current_actions;
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    // Right now, we can consolidate these actions because we only want to show
    // one action per extension. If clicking on an active script action ever
    // has a response, then we will need to split the actions.
    ExtensionAction* action =
        page_action_controller_->GetActionForExtension(*iter);
    if (!action)
      action = active_script_controller_->GetActionForExtension(*iter);
    if (action)
      current_actions.push_back(action);
  }

  return current_actions;
}

ExtensionAction::ShowAction LocationBarController::OnClicked(
    const ExtensionAction* action) {
  const Extension* extension =
      ExtensionRegistry::Get(web_contents_->GetBrowserContext())
          ->enabled_extensions().GetByID(action->extension_id());
  CHECK(extension) << action->extension_id();

  ExtensionAction::ShowAction page_action =
      page_action_controller_->GetActionForExtension(extension) ?
          page_action_controller_->OnClicked(extension) :
          ExtensionAction::ACTION_NONE;
  ExtensionAction::ShowAction active_script_action =
      active_script_controller_->GetActionForExtension(extension) ?
          active_script_controller_->OnClicked(extension) :
          ExtensionAction::ACTION_NONE;

  // PageAction response takes priority.
  return page_action != ExtensionAction::ACTION_NONE ? page_action :
                                                       active_script_action;
}

// static
void LocationBarController::NotifyChange(content::WebContents* web_contents) {
  web_contents->NotifyNavigationStateChanged(
      content::INVALIDATE_TYPE_PAGE_ACTIONS);
}

void LocationBarController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  page_action_controller_->OnNavigated();
  active_script_controller_->OnNavigated();
}

void LocationBarController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  bool should_update = false;
  if (page_action_controller_->GetActionForExtension(extension)) {
    page_action_controller_->OnExtensionUnloaded(extension);
    should_update = true;
  }
  if (active_script_controller_->GetActionForExtension(extension)) {
    active_script_controller_->OnExtensionUnloaded(extension);
    should_update = true;
  }

  if (should_update)
    NotifyChange(web_contents());
}

}  // namespace extensions
