// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/location_bar_controller.h"

#include <algorithm>

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

LocationBarController::LocationBarController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      action_manager_(ExtensionActionManager::Get(browser_context_)),
      should_show_page_actions_(
          !FeatureSwitch::extension_action_redesign()->IsEnabled()),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

LocationBarController::~LocationBarController() {
}

std::vector<ExtensionAction*> LocationBarController::GetCurrentActions() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  std::vector<ExtensionAction*> current_actions;
  if (!should_show_page_actions_)
    return current_actions;

  ExtensionActionRunner* action_executor =
      ExtensionActionRunner::GetForWebContents(web_contents_);
  for (const scoped_refptr<const Extension>& extension: extensions) {
    // Right now, we can consolidate these actions because we only want to show
    // one action per extension. If clicking on an active script action ever
    // has a response, then we will need to split the actions.
    ExtensionAction* action = action_manager_->GetPageAction(*extension);
    if (!action && action_executor->WantsToRun(extension.get())) {
      ExtensionActionMap::iterator existing =
          active_script_actions_.find(extension->id());
      if (existing != active_script_actions_.end()) {
        action = existing->second.get();
      } else {
        std::unique_ptr<ExtensionAction> active_script_action =
            ExtensionActionManager::Get(browser_context_)
                ->GetBestFitAction(*extension, ActionInfo::TYPE_PAGE);
        active_script_action->SetIsVisible(
            ExtensionAction::kDefaultTabId, true);
        action = active_script_action.get();
        active_script_actions_[extension->id()] =
            std::move(active_script_action);
      }
    }

    if (action)
      current_actions.push_back(action);
  }

  // Sort by id to guarantee the extension actions are returned in the same
  // order every time this function is called.
  std::sort(current_actions.begin(), current_actions.end(),
            [](ExtensionAction* a, ExtensionAction* b) {
              return a->extension_id() < b->extension_id();
            });
  // Move extensions with BookmarkManagerPrivate permission to the start. This
  // is to ensure they will always end up rightmost on the location bar.
  std::stable_partition(
      current_actions.begin(), current_actions.end(),
      [&extensions](ExtensionAction* extension_action) {
        return extensions.GetByID(extension_action->extension_id())
            ->permissions_data()
            ->HasAPIPermission(
                extensions::APIPermission::kBookmarkManagerPrivate);
      });
  return current_actions;
}

void LocationBarController::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (should_show_page_actions_ && action_manager_->GetPageAction(*extension)) {
    ExtensionActionAPI::Get(browser_context)->
        NotifyPageActionsChanged(web_contents_);
  }

  // We might also need to update the location bar if the extension can remove
  // the bookmark star.
  if (UIOverrides::RemovesBookmarkButton(extension)) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    // In a perfect world, this can never be NULL. Unfortunately, since a
    // LocationBarController is attached to most WebContents, we can't make that
    // guarantee.
    if (!browser)
      return;
    // window() can be NULL if this is called before CreateBrowserWindow()
    // completes, and there won't be a location bar if the window has no toolbar
    // (e.g., and app window).
    LocationBar* location_bar =
        browser->window() ? browser->window()->GetLocationBar() : NULL;
    if (!location_bar)
      return;
    location_bar->UpdateBookmarkStarVisibility();
  }
}

void LocationBarController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (should_show_page_actions_ && action_manager_->GetPageAction(*extension)) {
    ExtensionActionAPI::Get(browser_context)->
        NotifyPageActionsChanged(web_contents_);
  }
}

}  // namespace extensions
