// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct KioskModeInfo : public Extension::ManifestData {
 public:
  enum KioskStatus {
    NONE,
    ENABLED,
    ONLY
  };

  explicit KioskModeInfo(KioskStatus kiosk_status);
  virtual ~KioskModeInfo();

  KioskStatus kiosk_status;

  // Whether the extension or app is enabled for app kiosk mode.
  static bool IsKioskEnabled(const Extension* extension);

  // Whether the extension or app should only be available in kiosk mode.
  static bool IsKioskOnly(const Extension* extension);
};

// Parses the "kiosk_enabled" and "kiosk_only" manifest keys.
class KioskModeHandler : public ManifestHandler {
 public:
  KioskModeHandler();
  virtual ~KioskModeHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  std::vector<std::string> supported_keys_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_
