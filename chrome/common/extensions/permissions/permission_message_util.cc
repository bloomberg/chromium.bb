// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permission_message_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permission_message.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::PermissionMessage;

namespace permission_message_util {

PermissionMessage CreateFromHostList(const std::set<std::string>& hosts) {
  std::vector<std::string> host_list(hosts.begin(), hosts.end());
  DCHECK(host_list.size());
  PermissionMessage::ID message_id;
  string16 message;
  string16 details;

  switch (host_list.size()) {
    case 1:
      message_id = PermissionMessage::kHosts1;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_1_HOST,
                                           UTF8ToUTF16(host_list[0]));
      break;
    case 2:
      message_id = PermissionMessage::kHosts2;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]));
      break;
    case 3:
      message_id = PermissionMessage::kHosts3;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]),
                                           UTF8ToUTF16(host_list[2]));
      break;
    default:
      message_id = PermissionMessage::kHosts4OrMore;

      const int kRetainedFilesMessageIDs[6] = {
          IDS_EXTENSION_PROMPT_WARNING_HOSTS_DEFAULT,
          IDS_EXTENSION_PROMPT_WARNING_HOST_SINGULAR,
          IDS_EXTENSION_PROMPT_WARNING_HOSTS_ZERO,
          IDS_EXTENSION_PROMPT_WARNING_HOSTS_TWO,
          IDS_EXTENSION_PROMPT_WARNING_HOSTS_FEW,
          IDS_EXTENSION_PROMPT_WARNING_HOSTS_MANY,
      };
      std::vector<int> message_ids;
      for (size_t i = 0; i < arraysize(kRetainedFilesMessageIDs); i++) {
        message_ids.push_back(kRetainedFilesMessageIDs[i]);
      }
      message = l10n_util::GetPluralStringFUTF16(message_ids, host_list.size());

      for (size_t i = 0; i < host_list.size(); ++i) {
        if (i > 0)
          details += ASCIIToUTF16("\n");
        details += l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_HOST_LIST_ENTRY,
            UTF8ToUTF16(host_list[i]));
      }
  }

  return PermissionMessage(message_id, message, details);
}

}  // namespace permission_message_util
