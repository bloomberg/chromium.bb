// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_test_util.h"

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"

namespace extensions {
namespace extension_action_test_util {

namespace {

size_t GetPageActionCount(content::WebContents* web_contents,
                          bool only_count_visible) {
  DCHECK(web_contents);
  size_t count = 0u;
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  // Page actions are either stored in the location bar (and provided by the
  // LocationBarController), or in the main toolbar (and provided by the
  // ExtensionToolbarModel), depending on whether or not the extension action
  // redesign is enabled.
  if (!FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    std::vector<ExtensionAction*> page_actions =
        TabHelper::FromWebContents(web_contents)->
            location_bar_controller()->GetCurrentActions();
    count = page_actions.size();
    // Trim any invisible page actions, if necessary.
    if (only_count_visible) {
      for (std::vector<ExtensionAction*>::iterator iter = page_actions.begin();
           iter != page_actions.end(); ++iter) {
        if (!(*iter)->GetIsVisible(tab_id))
          --count;
      }
    }
  } else {
    ExtensionToolbarModel* toolbar_model =
        ExtensionToolbarModel::Get(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    const ExtensionList& toolbar_extensions = toolbar_model->toolbar_items();
    ExtensionActionManager* action_manager =
        ExtensionActionManager::Get(web_contents->GetBrowserContext());
    for (ExtensionList::const_iterator iter = toolbar_extensions.begin();
         iter != toolbar_extensions.end(); ++iter) {
      ExtensionAction* extension_action = action_manager->GetPageAction(**iter);
      if (extension_action &&
          (!only_count_visible || extension_action->GetIsVisible(tab_id)))
        ++count;
    }
  }

  return count;
}

}  // namespace

size_t GetVisiblePageActionCount(content::WebContents* web_contents) {
  return GetPageActionCount(web_contents, true);
}

size_t GetTotalPageActionCount(content::WebContents* web_contents) {
  return GetPageActionCount(web_contents, false);
}

}  // namespace extension_action_test_util
}  // namespace extensions
