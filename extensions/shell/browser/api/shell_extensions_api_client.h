// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_SHELL_EXTENSIONS_API_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_API_SHELL_EXTENSIONS_API_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {

// Extra support for Chrome extensions APIs in app_shell.
class ShellExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  ShellExtensionsAPIClient();
  virtual ~ShellExtensionsAPIClient();

  // ExtensionsAPIClient implementation.
  virtual device::HidService* GetHidService() OVERRIDE;

 private:
  scoped_ptr<device::HidService> hid_service_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_SHELL_EXTENSIONS_API_CLIENT_H_
