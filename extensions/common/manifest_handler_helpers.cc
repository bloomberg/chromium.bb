// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler_helpers.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"


namespace extensions {

namespace errors = manifest_errors;

namespace manifest_handler_helpers {

bool NormalizeAndValidatePath(std::string* path) {
  size_t first_non_slash = path->find_first_not_of('/');
  if (first_non_slash == std::string::npos) {
    *path = "";
    return false;
  }

  *path = path->substr(first_non_slash);
  return true;
}

bool LoadIconsFromDictionary(const base::DictionaryValue* icons_value,
                             const int* icon_sizes,
                             size_t num_icon_sizes,
                             ExtensionIconSet* icons,
                             base::string16* error) {
  DCHECK(icons);
  for (size_t i = 0; i < num_icon_sizes; ++i) {
    std::string key = base::IntToString(icon_sizes[i]);
    if (icons_value->HasKey(key)) {
      std::string icon_path;
      if (!icons_value->GetString(key, &icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      if (!NormalizeAndValidatePath(&icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      icons->Add(icon_sizes[i], icon_path);
    }
  }
  return true;
}

}  // namespace manifest_handler_helpers

}  // namespace extensions
