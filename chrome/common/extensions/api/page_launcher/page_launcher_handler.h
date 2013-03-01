// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {
class Extension;

// Parses the "page_launcher" manifest key.
class PageLauncherHandler : public ManifestHandler {
 public:
  PageLauncherHandler();
  virtual ~PageLauncherHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PageLauncherHandler);
};
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_HANDLER_H_
