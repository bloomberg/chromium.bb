// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui_android.h"

#include "base/logging.h"

void ExtensionInstallUIAndroid::OnInstallSuccess(
    const extensions::Extension* extension, SkBitmap* icon) {
  NOTIMPLEMENTED();
}

void ExtensionInstallUIAndroid::OnInstallFailure(
    const CrxInstallerError& error) {
  NOTIMPLEMENTED();
}

void ExtensionInstallUIAndroid::SetSkipPostInstallUI(bool skip_ui) {
  NOTIMPLEMENTED();
}

// static
ExtensionInstallUI* ExtensionInstallUI::Create(Browser* browser) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
void ExtensionInstallUI::OpenAppInstalledUI(
    Browser* browser, const std::string& app_id) {
  NOTIMPLEMENTED();
}

// static
void ExtensionInstallUI::DisableFailureUIForTests() {
  NOTIMPLEMENTED();
}
