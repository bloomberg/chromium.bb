// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_BROWSER_ACTION_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_BROWSER_ACTION_HANDLER_H_

#include <string>

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "browser_action" manifest key.
class BrowserActionHandler : public ManifestHandler {
 public:
  BrowserActionHandler();
  virtual ~BrowserActionHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_BROWSER_ACTION_HANDLER_H_
