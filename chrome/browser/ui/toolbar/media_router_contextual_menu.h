// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

// The class for the contextual menu for the Media Router action.
class MediaRouterContextualMenu : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MediaRouterContextualMenu(Browser* browser);
  ~MediaRouterContextualMenu() override;

  ui::MenuModel* menu_model() { return &menu_model_; }

 private:
  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

  void ReportIssue();
  void RemoveMediaRouterComponentAction();

  Browser* browser_;
  ui::SimpleMenuModel menu_model_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenu);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
