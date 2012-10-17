// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_

class Browser;
class ExtensionService;

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
