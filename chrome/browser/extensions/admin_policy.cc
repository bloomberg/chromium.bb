// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/admin_policy.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool IsRequired(const extensions::Extension* extension) {
  return extensions::Extension::IsRequired(extension->location());
}

bool ManagementPolicyImpl(const extensions::Extension* extension,
                          string16* error,
                          bool modifiable_value) {
  bool modifiable = !IsRequired(extension);
  // Some callers equate "no restriction" to true, others to false.
  if (modifiable)
    return modifiable_value;

  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_MODIFY_POLICY_REQUIRED,
        UTF8ToUTF16(extension->name()));
  }
  return !modifiable_value;
}

}  // namespace

namespace extensions {
namespace admin_policy {

bool BlacklistedByDefault(const base::ListValue* blacklist) {
  base::StringValue wildcard("*");
  return blacklist && blacklist->Find(wildcard) != blacklist->end();
}

bool UserMayLoad(const base::ListValue* blacklist,
                 const base::ListValue* whitelist,
                 const Extension* extension,
                 string16* error) {
  if (IsRequired(extension))
    return true;

  if (!blacklist || blacklist->empty())
    return true;

  // Check the whitelist first.
  base::StringValue id_value(extension->id());
  if (whitelist && whitelist->Find(id_value) != whitelist->end())
    return true;

  // Then check the blacklist (the admin blacklist, not the Google blacklist).
  bool result = blacklist->Find(id_value) == blacklist->end() &&
      !BlacklistedByDefault(blacklist);
  if (error && !result) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_POLICY_BLACKLIST,
          UTF8ToUTF16(extension->name()),
          UTF8ToUTF16(extension->id()));
  }
  return result;
}

bool UserMayModifySettings(const Extension* extension, string16* error) {
  return ManagementPolicyImpl(extension, error, true);
}

bool MustRemainEnabled(const Extension* extension, string16* error) {
  return ManagementPolicyImpl(extension, error, false);
}

}  // namespace
}  // namespace
