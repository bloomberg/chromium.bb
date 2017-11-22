// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_PLUGINS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_PLUGINS_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "plugins" manifest key.
class PluginsHandler : public ManifestHandler {
 public:
  PluginsHandler();
  ~PluginsHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  const std::vector<std::string> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(PluginsHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_PLUGINS_HANDLER_H_
