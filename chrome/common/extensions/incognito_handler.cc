// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/incognito_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace keys = extension_manifest_keys;

namespace extensions {

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

bool IncognitoHandler::Parse(Extension* extension, string16* error) {
  if (!extension->manifest()->HasKey(keys::kIncognito)) {
    // Apps default to split mode, extensions default to spanning.
    extension->SetManifestData(keys::kIncognito,
                               new IncognitoInfo(extension->is_app()));
    return true;
  }

  bool split_mode = false;
  std::string incognito_string;
  if (!extension->manifest()->GetString(keys::kIncognito, &incognito_string)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidIncognitoBehavior);
    return false;
  }

  if (incognito_string == extension_manifest_values::kIncognitoSplit)
    split_mode = true;
  else if (incognito_string != extension_manifest_values::kIncognitoSpanning) {
    // If incognito_string == kIncognitoSpanning, it is valid and
    // split_mode remains false.
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidIncognitoBehavior);
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
