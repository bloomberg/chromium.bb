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
// was installed. If |is_new_profile| is true, then this error is from the
// first time our profile checked for new external extensions.
bool AddExternalInstallError(ExtensionService* service,
                             const Extension* extension,
                             bool is_new_profile);
void RemoveExternalInstallError(ExtensionService* service);

// Used for testing.
bool HasExternalInstallError(ExtensionService* service);
bool HasExternalInstallBubble(ExtensionService* service);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_UI_H_
