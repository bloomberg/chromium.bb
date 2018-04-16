// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {
class Extension;
}

namespace extensions {

// Parses the "event_rules" manifest key.
class DeclarativeManifestHandler : public ManifestHandler {
 public:
  DeclarativeManifestHandler();
  ~DeclarativeManifestHandler() override;

  // ManifestHandler overrides.
  bool Parse(Extension* extension, base::string16* error) override;

 private:
  // ManifestHandler overrides.
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeManifestHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_
