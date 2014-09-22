// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_CONSTANTS_H_

#include <string>

#include "extensions/common/manifest.h"

namespace extensions {
namespace schema_constants {

extern const char kWildcard[];

extern const char kInstallationMode[];
extern const char kAllowed[];
extern const char kBlocked[];
extern const char kForceInstalled[];
extern const char kNormalInstalled[];

extern const char kUpdateUrl[];
extern const char kInstallSources[];
extern const char kAllowedTypes[];

extern const char kUpdateUrlPrefix[];

struct AllowedTypesMapEntry {
  // Name of allowed types of extensions used in schema of extension
  // management preference.
  const char* name;
  // The corresponding Manifest::Type.
  Manifest::Type manifest_type;
};

extern const size_t kAllowedTypesMapSize;
extern const AllowedTypesMapEntry kAllowedTypesMap[];

// Helper fuction over |kAllowedTypesMap|, returns Manifest::TYPE_UNKNOWN if
// not found.
Manifest::Type GetManifestType(const std::string& name);

}  // namespace schema_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_CONSTANTS_H_
