// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/incognito_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

IncognitoInfo::IncognitoInfo(bool incognito_split_mode)
    : split_mode(incognito_split_mode) {
}

IncognitoInfo::~IncognitoInfo() {
}

// static
bool IncognitoInfo::IsSplitMode(const Extension* extension) {
  IncognitoInfo* info = static_cast<IncognitoInfo*>(
      extension->GetManifestData(keys::kIncognito));
  return info ? info->split_mode : false;
}

IncognitoHandler::IncognitoHandler() {
}

IncognitoHandler::~IncognitoHandler() {
}

bool IncognitoHandler::Parse(Extension* extension, base::string16* error) {
  if (!extension->manifest()->HasKey(keys::kIncognito)) {
    // Extensions and Chrome apps default to spanning mode.
    // Hosted and legacy packaged apps default to split mode.
    extension->SetManifestData(
        keys::kIncognito,
        new IncognitoInfo(extension->is_hosted_app() ||
                          extension->is_legacy_packaged_app()));
    return true;
  }

  bool split_mode = false;
  std::string incognito_string;
  if (!extension->manifest()->GetString(keys::kIncognito, &incognito_string)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidIncognitoBehavior);
    return false;
  }

  if (incognito_string == manifest_values::kIncognitoSplit)
    split_mode = true;
  else if (incognito_string != manifest_values::kIncognitoSpanning) {
    // If incognito_string == kIncognitoSpanning, it is valid and
    // split_mode remains false.
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidIncognitoBehavior);
    return false;
  }

  extension->SetManifestData(keys::kIncognito, new IncognitoInfo(split_mode));
  return true;
}

bool IncognitoHandler::AlwaysParseForType(Manifest::Type type) const {
  return true;
}

const std::vector<std::string> IncognitoHandler::Keys() const {
  return SingleKey(keys::kIncognito);
}

}  // namespace extensions
