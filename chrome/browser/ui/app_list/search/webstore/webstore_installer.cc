// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore/webstore_installer.h"

namespace app_list {

WebstoreInstaller::WebstoreInstaller(const std::string& webstore_item_id,
                                     Profile* profile,
                                     gfx::NativeWindow parent_window,
                                     const Callback& callback)
    : WebstoreInstallWithPrompt(webstore_item_id,
                                profile,
                                parent_window,
                                callback) {
  set_install_source(
      extensions::WebstoreInstaller::INSTALL_SOURCE_APP_LAUNCHER);
}

WebstoreInstaller::~WebstoreInstaller() {}

}  // namespace app_list
