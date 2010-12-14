// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_
#pragma once

class Extension;
class ExtensionService;
class Profile;

// Shows UI to inform the user that an extension was disabled after upgrading
// to higher permissions.
void ShowExtensionDisabledUI(ExtensionService* service, Profile* profile,
                             const Extension* extension);

// Shows the extension install dialog.
void ShowExtensionDisabledDialog(ExtensionService* service, Profile* profile,
                                 const Extension* extension);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DISABLED_INFOBAR_DELEGATE_H_
