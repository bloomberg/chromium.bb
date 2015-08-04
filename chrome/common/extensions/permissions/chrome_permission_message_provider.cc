// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include "base/memory/scoped_vector.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

typedef std::set<PermissionMessage> PermissionMsgSet;

ChromePermissionMessageProvider::ChromePermissionMessageProvider() {
}

ChromePermissionMessageProvider::~ChromePermissionMessageProvider() {
}

CoalescedPermissionMessages
ChromePermissionMessageProvider::GetPermissionMessages(
    const PermissionIDSet& permissions) const {
  std::vector<ChromePermissionMessageRule> rules =
      ChromePermissionMessageRule::GetAllRules();

  // Apply each of the rules, in order, to generate the messages for the given
  // permissions. Once a permission is used in a rule, remove it from the set
  // of available permissions so it cannot be applied to subsequent rules.
  PermissionIDSet remaining_permissions = permissions;
  CoalescedPermissionMessages messages;
  for (const auto& rule : rules) {
    // Only apply the rule if we have all the required permission IDs.
    if (remaining_permissions.ContainsAllIDs(rule.required_permissions())) {
      // We can apply the rule. Add all the required permissions, and as many
      // optional permissions as we can, to the new message.
      PermissionIDSet used_permissions =
          remaining_permissions.GetAllPermissionsWithIDs(
              rule.all_permissions());
      messages.push_back(rule.GetPermissionMessage(used_permissions));

      remaining_permissions =
          PermissionIDSet::Difference(remaining_permissions, used_permissions);
    }
  }

  return messages;
}

bool ChromePermissionMessageProvider::IsPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions,
    Manifest::Type extension_type) const {
  // Things can't get worse than native code access.
  if (old_permissions->HasEffectiveFullAccess())
    return false;

  // Otherwise, it's a privilege increase if the new one has full access.
  if (new_permissions->HasEffectiveFullAccess())
    return true;

  if (IsHostPrivilegeIncrease(old_permissions, new_permissions, extension_type))
    return true;

  if (IsAPIPrivilegeIncrease(old_permissions, new_permissions, extension_type))
    return true;

  if (IsManifestPermissionPrivilegeIncrease(old_permissions, new_permissions))
    return true;

  return false;
}

PermissionIDSet ChromePermissionMessageProvider::GetAllPermissionIDs(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  PermissionIDSet permission_ids;
  GetAPIPermissionMessages(permissions, &permission_ids, extension_type);
  GetManifestPermissionMessages(permissions, &permission_ids);
  GetHostPermissionMessages(permissions, &permission_ids, extension_type);
  return permission_ids;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetAPIPermissionMessages(
    const PermissionSet* permissions,
    PermissionIDSet* permission_ids,
    Manifest::Type extension_type) const {
  PermissionMsgSet messages;
  for (APIPermissionSet::const_iterator permission_it =
           permissions->apis().begin();
       permission_it != permissions->apis().end(); ++permission_it) {
    if (permission_ids != NULL)
      permission_ids->InsertAll(permission_it->GetPermissions());
    if (permission_it->HasMessages()) {
      PermissionMessages new_messages = permission_it->GetMessages();
      messages.insert(new_messages.begin(), new_messages.end());
    }
  }

  // A special hack: The warning message for declarativeWebRequest
  // permissions speaks about blocking parts of pages, which is a
  // subset of what the "<all_urls>" access allows. Therefore we
  // display only the "<all_urls>" warning message if both permissions
  // are required.
  if (permissions->ShouldWarnAllHosts()) {
    // Platform apps don't show hosts warnings. See crbug.com/255229.
    if (permission_ids != NULL && extension_type != Manifest::TYPE_PLATFORM_APP)
      permission_ids->insert(APIPermission::kHostsAll);
    messages.erase(
        PermissionMessage(
            PermissionMessage::kDeclarativeWebRequest, base::string16()));
  }
  return messages;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetManifestPermissionMessages(
    const PermissionSet* permissions,
    PermissionIDSet* permission_ids) const {
  PermissionMsgSet messages;
  for (ManifestPermissionSet::const_iterator permission_it =
           permissions->manifest_permissions().begin();
      permission_it != permissions->manifest_permissions().end();
      ++permission_it) {
    if (permission_ids != NULL)
      permission_ids->InsertAll(permission_it->GetPermissions());
    if (permission_it->HasMessages()) {
      PermissionMessages new_messages = permission_it->GetMessages();
      messages.insert(new_messages.begin(), new_messages.end());
    }
  }
  return messages;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetHostPermissionMessages(
    const PermissionSet* permissions,
    PermissionIDSet* permission_ids,
    Manifest::Type extension_type) const {
  PermissionMsgSet messages;
  // Since platform apps always use isolated storage, they can't (silently)
  // access user data on other domains, so there's no need to prompt.
  // Note: this must remain consistent with IsHostPrivilegeIncrease.
  // See crbug.com/255229.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return messages;

  if (permissions->ShouldWarnAllHosts()) {
    if (permission_ids != NULL)
      permission_ids->insert(APIPermission::kHostsAll);
    messages.insert(PermissionMessage(
        PermissionMessage::kHostsAll,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS)));
  } else {
    URLPatternSet regular_hosts;
    ExtensionsClient::Get()->FilterHostPermissions(
        permissions->effective_hosts(), &regular_hosts, &messages);
    if (permission_ids != NULL) {
      ExtensionsClient::Get()->FilterHostPermissions(
          permissions->effective_hosts(), &regular_hosts, permission_ids);
    }

    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    if (!hosts.empty()) {
      if (permission_ids != NULL) {
        permission_message_util::AddHostPermissions(
            permission_ids, hosts, permission_message_util::kReadWrite);
      }
      messages.insert(permission_message_util::CreateFromHostList(
          hosts, permission_message_util::kReadWrite));
    }
  }
  return messages;
}

bool ChromePermissionMessageProvider::IsAPIPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions,
    Manifest::Type extension_type) const {
  if (new_permissions == NULL)
    return false;

  PermissionMsgSet old_warnings =
      GetAPIPermissionMessages(old_permissions, NULL, extension_type);
  PermissionMsgSet new_warnings =
      GetAPIPermissionMessages(new_permissions, NULL, extension_type);
  PermissionMsgSet delta_warnings =
      base::STLSetDifference<PermissionMsgSet>(new_warnings, old_warnings);

  // A special hack: kFileSystemWriteDirectory implies kFileSystemDirectory.
  // TODO(sammc): Remove this. See http://crbug.com/284849.
  if (old_warnings.find(PermissionMessage(
          PermissionMessage::kFileSystemWriteDirectory, base::string16())) !=
      old_warnings.end()) {
    delta_warnings.erase(
        PermissionMessage(PermissionMessage::kFileSystemDirectory,
                          base::string16()));
  }

  // It is a privilege increase if there are additional warnings present.
  return !delta_warnings.empty();
}

bool ChromePermissionMessageProvider::IsManifestPermissionPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions) const {
  if (new_permissions == NULL)
    return false;

  PermissionMsgSet old_warnings =
      GetManifestPermissionMessages(old_permissions, NULL);
  PermissionMsgSet new_warnings =
      GetManifestPermissionMessages(new_permissions, NULL);
  PermissionMsgSet delta_warnings =
      base::STLSetDifference<PermissionMsgSet>(new_warnings, old_warnings);

  // It is a privilege increase if there are additional warnings present.
  return !delta_warnings.empty();
}

bool ChromePermissionMessageProvider::IsHostPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions,
    Manifest::Type extension_type) const {
  // Platform apps host permission changes do not count as privilege increases.
  // Note: this must remain consistent with GetHostPermissionMessages.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return false;

  // If the old permission set can access any host, then it can't be elevated.
  if (old_permissions->HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the new permission set has full host access, then it must be
  // a privilege increase.
  if (new_permissions->HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& old_list = old_permissions->effective_hosts();
  const URLPatternSet& new_list = new_permissions->effective_hosts();

  // TODO(jstritar): This is overly conservative with respect to subdomains.
  // For example, going from *.google.com to www.google.com will be
  // considered an elevation, even though it is not (http://crbug.com/65337).
  std::set<std::string> new_hosts_set(
      permission_message_util::GetDistinctHosts(new_list, false, false));
  std::set<std::string> old_hosts_set(
      permission_message_util::GetDistinctHosts(old_list, false, false));
  std::set<std::string> new_hosts_only =
      base::STLSetDifference<std::set<std::string> >(new_hosts_set,
                                                     old_hosts_set);

  return !new_hosts_only.empty();
}

}  // namespace extensions
