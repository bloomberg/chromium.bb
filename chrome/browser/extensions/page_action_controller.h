// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#pragma once

#include <set>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/extensions/action_box_controller.h"
#include "chrome/browser/extensions/extension_tab_helper.h"

class ExtensionService;
class TabContentsWrapper;

namespace extensions {

// An ActionBoxController which corresponds to the page actions of an extension.
class PageActionController : public ActionBoxController,
                             public ExtensionTabHelper::Observer {
 public:
  PageActionController(TabContentsWrapper* tab_contents,
                       ExtensionTabHelper* tab_helper);
  virtual ~PageActionController();

  // ActionBoxController implementation.
  virtual scoped_ptr<std::vector<ExtensionAction*> > GetCurrentActions()
      OVERRIDE;
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) OVERRIDE;

  // ExtensionTabHelper::Observer implementation.
  virtual void OnPageActionStateChanged() OVERRIDE;

 private:
  // Gets the ExtensionService for |tab_contents_|.
  ExtensionService* GetExtensionService();

  TabContentsWrapper* tab_contents_;

  ExtensionTabHelper* tab_helper_;

  DISALLOW_COPY_AND_ASSIGN(PageActionController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
