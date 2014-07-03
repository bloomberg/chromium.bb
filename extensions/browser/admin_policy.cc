// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/admin_policy.h"

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool ManagementPolicyImpl(const extensions::Extension* extension,
                          base::string16* error,
                          bool modifiable_value) {
  bool modifiable =
      !extensions::Manifest::IsComponentLocation(extension->location()) &&
      !extensions::Manifest::IsPolicyLocation(extension->location());
  // Some callers equate "no restriction" to true, others to false.
  if (modifiable)
    return modifiable_value;

  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_MODIFY_POLICY_REQUIRED,
        base::UTF8ToUTF16(extension->name()));
  }
  return !modifiable_value;
}

bool ReturnLoadError(const extensions::Extension* extension,
                     base::string16* error) {
  if (error) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_POLICY_BLOCKED,
          base::UTF8ToUTF16(extension->name()),
          base::UTF8ToUTF16(extension->id()));
  }
  return false;
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
                 const base::DictionaryValue* forcelist,
                 const base::ListValue* allowed_types,
                 const Extension* extension,
                 base::string16* error) {
  // Component extensions are always allowed.
  if (extension->location() == Manifest::COMPONENT)
    return true;

  // Forced installed extensions cannot be overwritten manually.
  if (extension->location() != Manifest::EXTERNAL_POLICY &&
      extension->location() != Manifest::EXTERNAL_POLICY_DOWNLOAD &&
      forcelist && forcelist->HasKey(extension->id())) {
    return ReturnLoadError(extension, error);
  }

  // Early exit for the common case of no policy restrictions.
  if ((!blacklist || blacklist->empty()) && (!allowed_types))
    return true;

  // Check whether the extension type is allowed.
  //
  // If you get a compile error here saying that the type you added is not
  // handled by the switch statement below, please consider whether enterprise
  // policy should be able to disallow extensions of the new type. If so, add a
  // branch to the second block and add a line to the definition of
  // kExtensionAllowedTypesMap in configuration_policy_handler_list.cc.
  switch (extension->GetType()) {
    case Manifest::TYPE_UNKNOWN:
      break;
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_THEME:
    case Manifest::TYPE_USER_SCRIPT:
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
    case Manifest::TYPE_SHARED_MODULE: {
      base::FundamentalValue type_value(extension->GetType());
      if (allowed_types &&
          allowed_types->Find(type_value) == allowed_types->end())
        return ReturnLoadError(extension, error);
      break;
    }
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }

  // Check the whitelist/forcelist first.
  base::StringValue id_value(extension->id());
  if ((whitelist && whitelist->Find(id_value) != whitelist->end()) ||
      (forcelist && forcelist->HasKey(extension->id())))
    return true;

  // Then check the admin blacklist.
  if ((blacklist && blacklist->Find(id_value) != blacklist->end()) ||
      BlacklistedByDefault(blacklist))
    return ReturnLoadError(extension, error);

  return true;
}

bool UserMayModifySettings(const Extension* extension, base::string16* error) {
  return ManagementPolicyImpl(extension, error, true);
}

bool MustRemainEnabled(const Extension* extension, base::string16* error) {
  return ManagementPolicyImpl(extension, error, false);
}

}  // namespace admin_policy
}  // namespace extensions
