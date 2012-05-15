// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#pragma once

#include "chrome/browser/extensions/action_box_controller.h"

class ExtensionService;
class TabContentsWrapper;

namespace extensions {

// An ActionBoxController which corresponds to the page actions of an extension.
class PageActionController : public ActionBoxController {
 public:
  explicit PageActionController(TabContentsWrapper* tab_contents);
  virtual ~PageActionController();

  virtual scoped_ptr<DataList> GetAllBadgeData() OVERRIDE;

  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) OVERRIDE;

 private:
  // Gets the ExtensionService for |tab_contents_|.
  ExtensionService* GetExtensionService();

  TabContentsWrapper* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageActionController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
