// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_KIOSK_ENABLED_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_KIOSK_ENABLED_INFO_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

struct KioskEnabledInfo : public Extension::ManifestData {
  explicit KioskEnabledInfo(bool is_kiosk_enabled);
  virtual ~KioskEnabledInfo();

  bool kiosk_enabled;

  // Whether the extension or app should be enabled in app kiosk mode.
  static bool IsKioskEnabled(const Extension* extension);
};

// Parses the "kiosk_enabled" manifest key.
class KioskEnabledHandler : public ManifestHandler {
 public:
  KioskEnabledHandler();
  virtual ~KioskEnabledHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KioskEnabledHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_KIOSK_ENABLED_INFO_H_
