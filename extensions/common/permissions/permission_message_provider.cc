// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_provider.h"

#include "base/strings/string_split.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

PermissionMessageString::PermissionMessageString(
    const CoalescedPermissionMessage& message)
    : message(message.message()), submessages(message.submessages()) {
}

PermissionMessageString::PermissionMessageString(const base::string16& message,
                                                 const base::string16& details)
    : message(message) {
  base::SplitString(details, base::char16('\n'), &submessages);
}

PermissionMessageString::~PermissionMessageString() {
}

// static
const PermissionMessageProvider* PermissionMessageProvider::Get() {
  return &(ExtensionsClient::Get()->GetPermissionMessageProvider());
}

}  // namespace extensions
