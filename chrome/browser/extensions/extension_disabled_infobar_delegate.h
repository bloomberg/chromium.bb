// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/extensions/extension_resource.h"

class Extension;
class ExtensionsService;
class Profile;

// Shows UI to inform the user that an extension was disabled after upgrading
// to higher permissions.
void ShowExtensionDisabledUI(ExtensionsService* service, Profile* profile,
                             Extension* extension);

// Shows the extension install dialog.
void ShowExtensionDisabledDialog(ExtensionsService* service, Profile* profile,
                                 Extension* extension);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_
