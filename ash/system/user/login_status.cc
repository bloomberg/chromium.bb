// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/login_status.h"

#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace user {

string16 GetLocalizedSignOutStringForStatus(LoginStatus status) {
  int message_id;
  switch (status) {
    case LOGGED_IN_GUEST:
      message_id = IDS_ASH_STATUS_TRAY_EXIT_GUEST;
      break;
    case LOGGED_IN_KIOSK:
      message_id = IDS_ASH_STATUS_TRAY_EXIT_KIOSK;
      break;
    case LOGGED_IN_PUBLIC:
      message_id = IDS_ASH_STATUS_TRAY_EXIT_PUBLIC;
      break;
    default:
      message_id = IDS_ASH_STATUS_TRAY_SIGN_OUT;
      break;
  }
  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

}  // namespace user
}  // namespace ash
