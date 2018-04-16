// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct IncognitoInfo : public Extension::ManifestData {
  enum Mode { SPLIT, SPANNING, NOT_ALLOWED };

  explicit IncognitoInfo(Mode mode);

  ~IncognitoInfo() override;

  // If true, a separate process will be used for the extension in incognito
  // mode.
  Mode mode;

  // Return the incognito mode information for the given |extension|.
  static bool IsSplitMode(const Extension* extension);

  // Return whether this extension can be run in incognito mode as specified
  // in its manifest.
  static bool IsIncognitoAllowed(const Extension* extension);
};

// Parses the "incognito" manifest key.
class IncognitoHandler : public ManifestHandler {
 public:
  IncognitoHandler();
  ~IncognitoHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(IncognitoHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
