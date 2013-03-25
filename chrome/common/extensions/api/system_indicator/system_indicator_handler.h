// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_

#include "base/string16.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// Parses the "system_indicator" manifest key.
class SystemIndicatorHandler : public ManifestHandler {
 public:
  SystemIndicatorHandler();
  virtual ~SystemIndicatorHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_
