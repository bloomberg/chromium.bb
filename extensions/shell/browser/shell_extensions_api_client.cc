// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_api_client.h"

#include "extensions/shell/browser/shell_app_view_guest_delegate.h"

namespace extensions {

ShellExtensionsAPIClient::ShellExtensionsAPIClient() {
}

AppViewGuestDelegate* ShellExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ShellAppViewGuestDelegate();
}

}  // namespace extensions
