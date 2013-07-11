// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permission_message.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
//
// PermissionMessage
//

// static
PermissionMessage PermissionMessage::CreateFromHostList(
    const std::set<std::string>& hosts) {
  std::vector<std::string> host_list(hosts.begin(), hosts.end());
  DCHECK_GT(host_list.size(), 0UL);
  ID message_id;
  string16 message;
  string16 details;

  switch (host_list.size()) {
    case 1:
      message_id = kHosts1;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_1_HOST,
                                           UTF8ToUTF16(host_list[0]));
      break;
    case 2:
      message_id = kHosts2;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]));
      break;
    case 3:
      message_id = kHosts3;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]),
                                           UTF8ToUTF16(host_list[2]));
      break;
    default:
      message_id = kHosts4OrMore;

#if defined(TOOLKIT_VIEWS)
    message = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PROMPT_WARNING_HOSTS,
        base::IntToString16(host_list.size()));

    for (size_t i = 0; i < host_list.size(); ++i) {
      if (i > 0)
        details += ASCIIToUTF16("\n");
      details += l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_WARNING_HOST_LIST_ENTRY,
          UTF8ToUTF16(host_list[i]));
    }
#else
    // TODO(finnur): Remove once all platforms support expandible sections.
    message = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PROMPT_WARNING_4_OR_MORE_HOSTS,
        UTF8ToUTF16(host_list[0]),
        UTF8ToUTF16(host_list[1]),
        base::IntToString16(hosts.size() - 2));
    break;
#endif
  }

  return PermissionMessage(message_id, message, details);
}

PermissionMessage::PermissionMessage(
    PermissionMessage::ID id, const string16& message)
  : id_(id),
    message_(message) {
}

PermissionMessage::PermissionMessage(
    PermissionMessage::ID id, const string16& message, const string16& details)
  : id_(id),
    message_(message),
    details_(details) {
}

PermissionMessage::~PermissionMessage() {}

}  // namespace extensions
