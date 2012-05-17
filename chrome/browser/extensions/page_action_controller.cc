// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/page_action_controller.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

PageActionController::PageActionController(TabContentsWrapper* tab_contents)
    : tab_contents_(tab_contents) {}

PageActionController::~PageActionController() {}

scoped_ptr<ActionBoxController::DataList>
PageActionController::GetAllBadgeData() {
  const ExtensionSet* extensions = GetExtensionService()->extensions();
  scoped_ptr<DataList> all_badge_data(new DataList());
  for (ExtensionSet::const_iterator i = extensions->begin();
       i != extensions->end(); ++i) {
    ExtensionAction* action = (*i)->page_action();
    if (action) {
      Data data = {
        DECORATION_NONE,
        action,
      };
      all_badge_data->push_back(data);
    }
  }
  return all_badge_data.Pass();
}

ActionBoxController::Action PageActionController::OnClicked(
    const std::string& extension_id, int mouse_button) {
  const Extension* extension =
      GetExtensionService()->extensions()->GetByID(extension_id);
  CHECK(extension);
  ExtensionAction* page_action = extension->page_action();
  CHECK(page_action);
  int tab_id = ExtensionTabUtil::GetTabId(tab_contents_->web_contents());

  switch (mouse_button) {
    case 1:
    case 2:
      if (page_action->HasPopup(tab_id))
        return ACTION_SHOW_POPUP;

      GetExtensionService()->browser_event_router()->PageActionExecuted(
          tab_contents_->profile(),
          extension->id(),
          page_action->id(),
          tab_id,
          tab_contents_->web_contents()->GetURL().spec(),
          mouse_button);
      return ACTION_NONE;

    case 3:
      return extension->ShowConfigureContextMenus() ?
          ACTION_SHOW_CONTEXT_MENU : ACTION_NONE;
  }

  return ACTION_NONE;
}

ExtensionService* PageActionController::GetExtensionService() {
  return ExtensionSystem::Get(tab_contents_->profile())->extension_service();
}

}  // namespace extensions
