// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

struct AppIsolationInfo : public Extension::ManifestData {
  explicit AppIsolationInfo(bool isolated_storage);
  virtual ~AppIsolationInfo();

  static bool HasIsolatedStorage(const Extension* extension);

  // Whether this extension requests isolated storage.
  bool has_isolated_storage;
};

// Parses the "isolation" manifest key.
class AppIsolationHandler : public ManifestHandler {
 public:
  AppIsolationHandler();
  virtual ~AppIsolationHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppIsolationHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_
