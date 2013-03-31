// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;

namespace extensions {

AppIsolationInfo::AppIsolationInfo(bool isolated_storage)
    : has_isolated_storage(isolated_storage) {
}

AppIsolationInfo::~AppIsolationInfo() {
}

// static
bool AppIsolationInfo::HasIsolatedStorage(const Extension* extension) {
  AppIsolationInfo* info = static_cast<AppIsolationInfo*>(
      extension->GetManifestData(keys::kIsolation));
  return info ? info->has_isolated_storage : false;
}

AppIsolationHandler::AppIsolationHandler() {
}

AppIsolationHandler::~AppIsolationHandler() {
}

bool AppIsolationHandler::Parse(Extension* extension, string16* error) {
  // Platform apps always get isolated storage.
  if (extension->is_platform_app()) {
    extension->SetManifestData(keys::kIsolation, new AppIsolationInfo(true));
    return true;
  }

  // Other apps only get it if it is requested _and_ experimental APIs are
  // enabled.
  if (!extension->is_app() ||
      !extension->initial_api_permissions()->count(
          APIPermission::kExperimental)) {
    return true;
  }

  const Value* tmp_isolation = NULL;
  // We should only be parsing if the extension has the key in the manifest,
  // or is a platform app (which we already handled).
  DCHECK(extension->manifest()->Get(keys::kIsolation, &tmp_isolation));

  const ListValue* isolation_list = NULL;
  if (!tmp_isolation->GetAsList(&isolation_list)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidIsolation);
    return false;
  }

  bool has_isolated_storage = false;
  for (size_t i = 0; i < isolation_list->GetSize(); ++i) {
    std::string isolation_string;
    if (!isolation_list->GetString(i, &isolation_string)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          extension_manifest_errors::kInvalidIsolationValue,
          base::UintToString(i));
      return false;
    }

    // Check for isolated storage.
    if (isolation_string == extension_manifest_values::kIsolatedStorage) {
      has_isolated_storage = true;
    } else {
      DLOG(WARNING) << "Did not recognize isolation type: " << isolation_string;
    }
  }

  if (has_isolated_storage)
    extension->SetManifestData(keys::kIsolation, new AppIsolationInfo(true));

  return true;
}

bool AppIsolationHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_PLATFORM_APP;
}

const std::vector<std::string> AppIsolationHandler::Keys() const {
  return SingleKey(keys::kIsolation);
}

}  // namespace extensions
