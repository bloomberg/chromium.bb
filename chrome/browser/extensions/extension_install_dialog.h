// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_install_ui.h"

class Extension;
class Profile;
class SkBitmap;

namespace base {
class DictionaryValue;
}

void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallUI::Delegate* delegate,
                                const Extension* extension,
                                SkBitmap* icon,
                                const ExtensionInstallUI::Prompt& prompt);

// The implementations of this function are platform-specific.
void ShowExtensionInstallDialogImpl(Profile* profile,
                                    ExtensionInstallUI::Delegate* delegate,
                                    const Extension* extension,
                                    SkBitmap* icon,
                                    const ExtensionInstallUI::Prompt& prompt);

// Wrapper around ShowExtensionInstallDialog that shows the install dialog for
// a given manifest (that corresponds to an extension about to be installed with
// ID |id|). If the name or description in the manifest is a localized
// placeholder, it may be overidden with |localized_name| or
// |localized_description| (which may be empty). The Extension instance
// that's parsed is returned via |dummy_extension|. |prompt| should be fully
// populated except for the permissions field, which will be extracted from the
// extension.
// Returns true if |dummy_extension| is valid and delegate methods will be
// called.
bool ShowExtensionInstallDialogForManifest(
    Profile *profile,
    ExtensionInstallUI::Delegate* delegate,
    const base::DictionaryValue* manifest,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    SkBitmap* icon,
    const ExtensionInstallUI::Prompt& prompt,
    scoped_refptr<Extension>* dummy_extension);

// For use only in tests - sets a flag that makes invocations of
// ShowExtensionInstallDialog* skip putting up a real dialog, and
// instead act as if the dialog choice was to proceed or abort.
void SetExtensionInstallDialogAutoConfirmForTests(
    bool should_proceed);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_DIALOG_H_
