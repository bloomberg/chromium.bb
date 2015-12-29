// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler_helpers.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

bool LoadIconsFromDictionary(Extension* extension,
                             const base::DictionaryValue* icons_value,
                             ExtensionIconSet* icons,
                             base::string16* error) {
  DCHECK(icons);
  DCHECK(error);
  for (base::DictionaryValue::Iterator iterator(*icons_value);
       !iterator.IsAtEnd(); iterator.Advance()) {
    int size = 0;
    std::string icon_path;
    if (!base::StringToInt(iterator.key(), &size) ||
        !iterator.value().GetAsString(&icon_path) ||
        !NormalizeAndValidatePath(&icon_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidIconPath,
                                                   iterator.key());
      return false;
    }

    // For backwards compatibility, only warn (don't error out) if an icon is
    // missing. Component extensions can skip this check as their icons are not
    // located on disk. Unpacked extensions skip this check and fail later
    // during validation if the file isn't present. See crbug.com/570249
    // TODO(estade|devlin): remove this workaround and let install fail in the
    // validate step a few releases after M49. See http://crbug.com/571193
    if (Manifest::IsComponentLocation(extension->location()) ||
        Manifest::IsUnpackedLocation(extension->location()) ||
        file_util::ValidateFilePath(
            extension->GetResource(icon_path).GetFilePath())) {
      icons->Add(size, icon_path);
    } else {
      extension->AddInstallWarning(InstallWarning(
          l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_ICON_FAILED,
                                    base::UTF8ToUTF16(icon_path)),
          std::string()));
    }
  }
  return true;
}

}  // namespace manifest_handler_helpers

}  // namespace extensions
