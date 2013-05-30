// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace extensions {
namespace api {

// Windows: Use RLZ implementation of GetMachineId.
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
#if defined(ENABLE_RLZ)
  std::string result;
  rlz_lib::GetMachineId(&result);
  callback.Run(result);
#else
  // Not implemented if RLZ not defined.
  callback.Run("");
#endif
}

}  // namespace api
}  // namespace extensions
