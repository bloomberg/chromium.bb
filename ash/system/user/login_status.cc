// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/login_status.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace user {

string16 GetLocalizedSignOutStringForStatus(LoginStatus status,
                                            bool multiline) {
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
  string16 message =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
  // Desirable line breaking points are marked using \n. As the resource
  // framework does not evaluate escape sequences, the \n need to be explicitly
  // handled. Depending on the value of |multiline|, actual line breaks or
  // spaces are substituted.
  string16 newline = multiline ? ASCIIToUTF16("\n") : ASCIIToUTF16(" ");
  ReplaceSubstringsAfterOffset(&message, 0, ASCIIToUTF16("\\n"), newline);
  return message;
}

}  // namespace user
}  // namespace ash
