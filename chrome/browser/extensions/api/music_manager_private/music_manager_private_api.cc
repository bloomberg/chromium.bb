// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/music_manager_private_api.h"

#include <memory>

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

using content::BrowserThread;

namespace {

const char kDeviceIdNotSupported[] =
    "Device ID API is not supported on this platform.";

}

namespace extensions {
namespace api {

MusicManagerPrivateGetDeviceIdFunction::
    MusicManagerPrivateGetDeviceIdFunction() {
}

MusicManagerPrivateGetDeviceIdFunction::
    ~MusicManagerPrivateGetDeviceIdFunction() {
}

bool MusicManagerPrivateGetDeviceIdFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DeviceId::GetDeviceId(
      this->extension_id(),
      base::Bind(
          &MusicManagerPrivateGetDeviceIdFunction::DeviceIdCallback,
          this));
  return true;  // Still processing!
}

void MusicManagerPrivateGetDeviceIdFunction::DeviceIdCallback(
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool response;
  if (device_id.empty()) {
    SetError(kDeviceIdNotSupported);
    response = false;
  } else {
    SetResult(std::make_unique<base::Value>(device_id));
    response = true;
  }

  SendResponse(response);
}

} // namespace api
} // namespace extensions
