// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_

#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct IncognitoInfo : public Extension::ManifestData {
  explicit IncognitoInfo(bool split_mode);
  virtual ~IncognitoInfo();

  // If true, a separate process will be used for the extension in incognito
  // mode.
  bool split_mode;

  // Return the incognito mode information for the given |extension|.
  static bool IsSplitMode(const Extension* extension);
};

// Parses the "incognito" manifest key.
class IncognitoHandler : public ManifestHandler {
 public:
  IncognitoHandler();
  virtual ~IncognitoHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(IncognitoHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
