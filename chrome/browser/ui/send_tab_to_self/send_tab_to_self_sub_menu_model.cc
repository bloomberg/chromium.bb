// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"

namespace send_tab_to_self {

SendTabToSelfSubMenuModel::SendTabToSelfSubMenuModel(Profile* profile)
    : ui::SimpleMenuModel(this) {
  Build(profile);
}

SendTabToSelfSubMenuModel::~SendTabToSelfSubMenuModel() {}

bool SendTabToSelfSubMenuModel::IsCommandIdEnabled(int command_id) const {
  // Only valid device names are shown, so all items are enabled.
  return true;
}

void SendTabToSelfSubMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  // TODO(crbug/959060): handle click action.
  if (command_id == IDC_SEND_TAB_TO_SELF) {
    NOTIMPLEMENTED();
  } else if (command_id == IDC_CONTENT_LINK_SEND_TAB_TO_SELF) {
    NOTIMPLEMENTED();
  }
  return;
}

void SendTabToSelfSubMenuModel::Build(Profile* profile) {
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  DCHECK(service);
  send_tab_to_self::SendTabToSelfModel* model =
      service->GetSendTabToSelfModel();
  DCHECK(model);
  std::map<std::string, send_tab_to_self::TargetDeviceInfo> map =
      model->GetTargetDeviceNameToCacheInfoMap();
  if (!map.empty()) {
    for (const auto& item : map) {
      AddItem(IDC_SEND_TAB_TO_SELF, base::UTF8ToUTF16(item.first));
    }
  }
  return;
}

}  //  namespace send_tab_to_self
