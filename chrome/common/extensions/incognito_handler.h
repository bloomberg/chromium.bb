// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_INCOGNITO_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_INCOGNITO_HANDLER_H_

#include "base/string16.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

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

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(IncognitoHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_INCOGNITO_HANDLER_H_
