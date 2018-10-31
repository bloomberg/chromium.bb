// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_shares_localized_strings_provider.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace smb_dialog {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static const struct {
    const char* name;
    int id;
  } localized_strings[] = {
      // TODO(baileyberro): Rename these resources since they are no longer in
      // settings.
      {"smbShareUrl", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_URL},
      {"smbShareName", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_NAME},
      {"smbShareUsername", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_USERNAME},
      {"smbSharePassword", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_PASSWORD},
      {"smbShareAuthenticationMethod",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_AUTHENTICATION_METHOD},
      {"smbShareStandardAuthentication",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_STANDARD_AUTHENTICATION},
      {"smbShareKerberosAuthentication",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_KERBEROS_AUTHENTICATION},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

}  // namespace smb_dialog
}  // namespace chromeos
