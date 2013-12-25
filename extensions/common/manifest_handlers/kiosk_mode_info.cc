// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

KioskModeInfo::KioskModeInfo(KioskStatus kiosk_status)
    : kiosk_status(kiosk_status) {
}

KioskModeInfo::~KioskModeInfo() {
}

// static
bool KioskModeInfo::IsKioskEnabled(const Extension* extension) {
  KioskModeInfo* info = static_cast<KioskModeInfo*>(
      extension->GetManifestData(keys::kKioskMode));
  return info ? info->kiosk_status != NONE : false;
}

// static
bool KioskModeInfo::IsKioskOnly(const Extension* extension) {
  KioskModeInfo* info = static_cast<KioskModeInfo*>(
      extension->GetManifestData(keys::kKioskMode));
  return info ? info->kiosk_status == ONLY : false;
}

KioskModeHandler::KioskModeHandler() {
  supported_keys_.push_back(keys::kKioskEnabled);
  supported_keys_.push_back(keys::kKioskOnly);
}

KioskModeHandler::~KioskModeHandler() {
}

bool KioskModeHandler::Parse(Extension* extension, base::string16* error) {
  const Manifest* manifest = extension->manifest();
  DCHECK(manifest->HasKey(keys::kKioskEnabled) ||
         manifest->HasKey(keys::kKioskOnly));

  bool kiosk_enabled = false;
  if (manifest->HasKey(keys::kKioskEnabled) &&
      !manifest->GetBoolean(keys::kKioskEnabled, &kiosk_enabled)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskEnabled);
    return false;
  }

  bool kiosk_only = false;
  if (manifest->HasKey(keys::kKioskOnly) &&
      !manifest->GetBoolean(keys::kKioskOnly, &kiosk_only)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskOnly);
    return false;
  }

  if (kiosk_only && !kiosk_enabled) {
    *error = base::ASCIIToUTF16(
        manifest_errors::kInvalidKioskOnlyButNotEnabled);
    return false;
  }

  // All other use cases should be already filtered out by manifest feature
  // checks.
  DCHECK(extension->is_platform_app());

  KioskModeInfo::KioskStatus kiosk_status = KioskModeInfo::NONE;
  if (kiosk_enabled)
    kiosk_status = kiosk_only ? KioskModeInfo::ONLY : KioskModeInfo::ENABLED;

  extension->SetManifestData(keys::kKioskMode,
      new KioskModeInfo(kiosk_status));

  return true;
}

const std::vector<std::string> KioskModeHandler::Keys() const {
  return supported_keys_;
}

}  // namespace extensions
