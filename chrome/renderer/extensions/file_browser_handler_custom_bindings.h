// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Custom bindings for the fileBrowserHandler API.
class FileBrowserHandlerCustomBindings : public ChromeV8Extension {
 public:
  FileBrowserHandlerCustomBindings();

 private:
  DISALLOW_COPY_AND_ASSIGN(FileBrowserHandlerCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_
