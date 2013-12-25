// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/offline_enabled_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

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

bool OfflineEnabledHandler::Parse(Extension* extension, base::string16* error) {
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
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidOfflineEnabled);
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
