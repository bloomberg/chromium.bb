// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_PAGE_ACTION_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_PAGE_ACTION_HANDLER_H_

#include <string>

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// Parses the "page_action" manifest key.
class PageActionHandler : public ManifestHandler {
 public:
  PageActionHandler();
  virtual ~PageActionHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PageActionHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_PAGE_ACTION_HANDLER_H_
