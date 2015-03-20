// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_provider.h"

#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

namespace {

ForceForTesting g_force_permission_system_for_testing =
    ForceForTesting::DONT_FORCE;

bool IsNewPermissionMessageSystemEnabled() {
  if (g_force_permission_system_for_testing != ForceForTesting::DONT_FORCE)
    return g_force_permission_system_for_testing == ForceForTesting::FORCE_NEW;
  const std::string group_name =
      base::FieldTrialList::FindFullName("PermissionMessageSystem");
  return group_name == "NewSystem";
}

}  // namespace

void ForcePermissionMessageSystemForTesting(
    ForceForTesting force) {
  g_force_permission_system_for_testing = force;
}

PermissionMessageString::PermissionMessageString(
    const CoalescedPermissionMessage& message)
    : message(message.message()), submessages(message.submessages()) {
}

PermissionMessageString::PermissionMessageString(const base::string16& message)
    : message(message) {
}

PermissionMessageString::PermissionMessageString(
    const base::string16& message,
    const std::vector<base::string16>& submessages)
    : message(message), submessages(submessages) {
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

PermissionMessageStrings
PermissionMessageProvider::GetPermissionMessageStrings(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  PermissionMessageStrings strings;
  if (IsNewPermissionMessageSystemEnabled()) {
    CoalescedPermissionMessages messages = GetCoalescedPermissionMessages(
        GetAllPermissionIDs(permissions, extension_type));
    for (const CoalescedPermissionMessage& msg : messages)
      strings.push_back(PermissionMessageString(msg));
  } else {
    std::vector<base::string16> messages =
        GetLegacyWarningMessages(permissions, extension_type);
    std::vector<base::string16> details =
        GetLegacyWarningMessagesDetails(permissions, extension_type);
    DCHECK_EQ(messages.size(), details.size());
    for (size_t i = 0; i < messages.size(); i++)
      strings.push_back(PermissionMessageString(messages[i], details[i]));
  }
  return strings;
}

}  // namespace extensions
