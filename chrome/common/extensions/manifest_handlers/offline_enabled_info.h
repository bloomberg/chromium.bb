// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_OFFLINE_ENABLED_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_OFFLINE_ENABLED_INFO_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

struct OfflineEnabledInfo : public Extension::ManifestData {
  explicit OfflineEnabledInfo(bool offline_enabled);
  virtual ~OfflineEnabledInfo();

  // Whether the extension or app should be enabled when offline.
  // Defaults to false, except for platform apps which are offline by default.
  bool offline_enabled;

  static bool IsOfflineEnabled(const Extension* extension);
};

// Parses the "offline_enabled" manifest key.
class OfflineEnabledHandler : public ManifestHandler {
 public:
  OfflineEnabledHandler();
  virtual ~OfflineEnabledHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(OfflineEnabledHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_OFFLINE_ENABLED_INFO_H_
