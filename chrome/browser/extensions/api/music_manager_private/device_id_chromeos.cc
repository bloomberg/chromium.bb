// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include "base/message_loop/message_loop.h"
#include "chromeos/cryptohome/system_salt_getter.h"

namespace extensions {
namespace api {

// ChromeOS: Use the System Salt.
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
  chromeos::SystemSaltGetter* c_home = chromeos::SystemSaltGetter::Get();
  std::string result = c_home->GetSystemSaltSync();
  if (result.empty()) {
    // cryptohome must not be running; re-request after a delay.
    const int64 kRequestSystemSaltDelayMs = 500;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DeviceId::GetMachineId, callback),
        base::TimeDelta::FromMilliseconds(kRequestSystemSaltDelayMs));
    return;
  }
  callback.Run(result);
}

}  // namespace api
}  // namespace extensions
