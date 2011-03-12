// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#pragma once

#include "chrome/browser/extensions/extension_install_ui.h"

class Extension;
class Profile;
class SkBitmap;

// The implementations of this function are platform-specific.
void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallUI::Delegate* delegate,
                                const Extension* extension,
                                SkBitmap* icon,
                                ExtensionInstallUI::PromptType type);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
