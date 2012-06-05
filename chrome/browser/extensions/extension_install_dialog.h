// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_install_ui.h"

class Profile;

namespace base {
class DictionaryValue;
}

void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallUI::Delegate* delegate,
                                const ExtensionInstallUI::Prompt& prompt);

// The implementations of this function are platform-specific.
void ShowExtensionInstallDialogImpl(Profile* profile,
                                    ExtensionInstallUI::Delegate* delegate,
                                    const ExtensionInstallUI::Prompt& prompt);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
