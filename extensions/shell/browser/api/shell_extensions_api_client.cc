// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_extensions_api_client.h"

#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"

namespace extensions {

ShellExtensionsAPIClient::ShellExtensionsAPIClient() {
}

ShellExtensionsAPIClient::~ShellExtensionsAPIClient() {
}

device::HidService* ShellExtensionsAPIClient::GetHidService() {
  if (!hid_service_) {
    hid_service_.reset(device::HidService::Create(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI)));
  }
  return hid_service_.get();
}

}  // namespace extensions
