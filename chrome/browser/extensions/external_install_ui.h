// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_

#include "build/build_config.h"

class Browser;
class ExtensionService;

// Only enable the external install UI on Windows and Mac, because those
// are the platforms where external installs are the biggest issue.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define ENABLE_EXTERNAL_INSTALL_UI 1
#else
#define ENABLE_EXTERNAL_INSTALL_UI 0
#endif

namespace extensions {

class Extension;

// Adds/Removes a global error informing the user that an external extension
// was installed.
bool AddExternalInstallError(ExtensionService* service,
                             const Extension* extension);
void RemoveExternalInstallError(ExtensionService* service);

// Used for testing.
bool HasExternalInstallError(ExtensionService* service);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_
