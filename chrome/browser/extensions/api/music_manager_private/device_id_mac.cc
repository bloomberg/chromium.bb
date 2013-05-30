// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

namespace extensions {
namespace api {

// MacOS: Not yet implemented.
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
  // Not implemented if RLZ not defined.
  callback.Run("");
}

}  // namespace api
}  // namespace extensions
