// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_virtual_file_request_service_provider_delegate.h"

namespace chromeos {

ChromeVirtualFileRequestServiceProviderDelegate::
    ChromeVirtualFileRequestServiceProviderDelegate() = default;

ChromeVirtualFileRequestServiceProviderDelegate::
    ~ChromeVirtualFileRequestServiceProviderDelegate() = default;

bool ChromeVirtualFileRequestServiceProviderDelegate::HandleReadRequest(
    const std::string& id,
    int64_t offset,
    int64_t size,
    base::ScopedFD pipe_write_end) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace chromeos
