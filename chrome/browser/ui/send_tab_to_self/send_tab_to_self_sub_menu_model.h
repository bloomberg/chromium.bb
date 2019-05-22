// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_

// This is the secondary menu model of send tab to self context menu item. This
// menu contains the imformation of valid devices, getting form
// SendTabToSelfModel. Every item of this menu is a valid device.

#include <vector>

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

class SendTabToSelfSubMenuModel : public ui::SimpleMenuModel,
                                  public ui::SimpleMenuModel::Delegate {
 public:
  static const int kMinCommandId = 2000;
  static const int kMaxCommandId = 2020;

  struct ValidDeviceItem;

  enum ShareSourceType { kTab, kLink };

  SendTabToSelfSubMenuModel(content::WebContents* tab,
                            const GURL& link_url = GURL());
  ~SendTabToSelfSubMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void Build(Profile* profile);
  void BuildDeviceItem(const std::string& device_name,
                       const std::string& guid,
                       int index);

  content::WebContents* tab_;
  ShareSourceType source_type_ = kTab;
  GURL link_url_;
  std::vector<ValidDeviceItem> valid_device_items_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfSubMenuModel);
};

}  //  namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
