// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/music_manager_private_api.h"
#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

using content::BrowserThread;

namespace {

const char kDeviceIDNotSupported[] =
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

bool MusicManagerPrivateGetDeviceIdFunction::RunImpl() {
  std::string salt = this->extension_id();
  std::string result = device_id::GetDeviceID(salt);
  bool response;
  if (result.empty()) {
    SetError(kDeviceIDNotSupported);
    response = false;
  } else {
    SetResult(Value::CreateStringValue(result));
    response = true;
  }

  SendResponse(response);
  return response;
}

} // namespace api

} // namespace extensions
