// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/kiosk_enabled_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace keys = extension_manifest_keys;

namespace extensions {

KioskEnabledInfo::KioskEnabledInfo(bool is_kiosk_enabled)
    : kiosk_enabled(is_kiosk_enabled) {
}

KioskEnabledInfo::~KioskEnabledInfo() {
}

// static
bool KioskEnabledInfo::IsKioskEnabled(const Extension* extension) {
  KioskEnabledInfo* info = static_cast<KioskEnabledInfo*>(
      extension->GetManifestData(keys::kKioskEnabled));
  return info ? info->kiosk_enabled : false;
}

KioskEnabledHandler::KioskEnabledHandler() {
}

KioskEnabledHandler::~KioskEnabledHandler() {
}

bool KioskEnabledHandler::Parse(Extension* extension, string16* error) {
  DCHECK(extension->manifest()->HasKey(keys::kKioskEnabled));

  bool kiosk_enabled = false;
  if (!extension->manifest()->GetBoolean(keys::kKioskEnabled, &kiosk_enabled)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidKioskEnabled);
    return false;
  }

  // All other use cases should be already filtered out by manifest feature
  // checks.
  DCHECK(extension->is_platform_app());

  extension->SetManifestData(keys::kKioskEnabled,
                             new KioskEnabledInfo(kiosk_enabled));
  return true;
}

const std::vector<std::string> KioskEnabledHandler::Keys() const {
  return SingleKey(keys::kKioskEnabled);
}

}  // namespace extensions
