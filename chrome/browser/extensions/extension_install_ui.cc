// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

// static
bool ExtensionInstallUI::disable_failure_ui_for_tests_ = false;

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile),
      skip_post_install_ui_(false) {}

ExtensionInstallUI::~ExtensionInstallUI() {}
