// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/offline_enabled_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace keys = extension_manifest_keys;

namespace extensions {

OfflineEnabledInfo::OfflineEnabledInfo(bool is_offline_enabled)
    : offline_enabled(is_offline_enabled) {
}

OfflineEnabledInfo::~OfflineEnabledInfo() {
}

// static
bool OfflineEnabledInfo::IsOfflineEnabled(const Extension* extension) {
  OfflineEnabledInfo* info = static_cast<OfflineEnabledInfo*>(
      extension->GetManifestData(keys::kOfflineEnabled));
  return info ? info->offline_enabled : false;
}

OfflineEnabledHandler::OfflineEnabledHandler() {
}

OfflineEnabledHandler::~OfflineEnabledHandler() {
}

bool OfflineEnabledHandler::Parse(Extension* extension, string16* error) {
  if (!extension->manifest()->HasKey(keys::kOfflineEnabled)) {
    // Only platform apps default to being enabled offline, and we should only
    // attempt parsing without a key present if it is a platform app.
    DCHECK(extension->is_platform_app());
    extension->SetManifestData(keys::kOfflineEnabled,
                               new OfflineEnabledInfo(true));
    return true;
  }

  bool offline_enabled = false;

  if (!extension->manifest()->GetBoolean(keys::kOfflineEnabled,
                                         &offline_enabled)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidOfflineEnabled);
    return false;
  }

  extension->SetManifestData(keys::kOfflineEnabled,
                             new OfflineEnabledInfo(offline_enabled));
  return true;
}

bool OfflineEnabledHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_PLATFORM_APP;
}

const std::vector<std::string> OfflineEnabledHandler::Keys() const {
  return SingleKey(keys::kOfflineEnabled);
}

}  // namespace extensions
