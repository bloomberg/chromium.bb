// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/copresence_manifest.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

CopresenceManifestHandler::CopresenceManifestHandler() {
}

CopresenceManifestHandler::~CopresenceManifestHandler() {
}

bool CopresenceManifestHandler::Parse(Extension* extension,
                                      base::string16* error) {
  const base::DictionaryValue* copresence_config = nullptr;

  if (!extension->manifest()->GetDictionary(manifest_keys::kCopresence,
                                            &copresence_config)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidCopresenceConfig);
    return false;
  }

  scoped_ptr<CopresenceManifestData> manifest_data(new CopresenceManifestData);
  if (!copresence_config->GetString(manifest_values::kApiKey,
                                    &manifest_data->api_key) ||
      manifest_data->api_key.empty()) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidCopresenceApiKey);
    return false;
  }

  extension->SetManifestData(manifest_keys::kCopresence,
                             manifest_data.release());
  return true;
}

const std::vector<std::string> CopresenceManifestHandler::Keys() const {
  return SingleKey(manifest_keys::kCopresence);
}

CopresenceManifestData::CopresenceManifestData() {
}

CopresenceManifestData::~CopresenceManifestData() {
}

}  // namespace extensions
