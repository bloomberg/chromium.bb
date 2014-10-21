// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"

#include "athena/extensions/public/extensions_delegate.h"
#include "extensions/browser/install/extension_install_ui.h"

namespace extensions {

scoped_ptr<ExtensionInstallUI> CreateExtensionInstallUI(
    content::BrowserContext* context) {
  return athena::ExtensionsDelegate::Get(context)->CreateExtensionInstallUI();
}

}  // namespace extensions
