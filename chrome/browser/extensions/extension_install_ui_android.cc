// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include "base/logging.h"

void ExtensionInstallUIAndroid::OnInstallSuccess(const Extension* extension,
                                                 SkBitmap* icon) {
  NOTIMPLEMENTED();
}

void ExtensionInstallUIAndroid::OnInstallFailure(const string16& error) {
  NOTIMPLEMENTED();
}

void ExtensionInstallUIAndroid::SetSkipPostInstallUI(bool skip_ui) {
  NOTIMPLEMENTED();
}

// static
ExtensionInstallUI* ExtensionInstallUI::Create(Profile* profile) {
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
