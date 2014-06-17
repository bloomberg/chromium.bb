// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

namespace {

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) {
  NOTIMPLEMENTED();
}

}  // namespace

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
