// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/hid/input_service_linux.h"
#include "device/hid/public/interfaces/input_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(InputServiceLinux, Simple) {
  base::MessageLoopForIO message_loop;
  base::FileDescriptorWatcher file_descriptor_watcher(&message_loop);

  InputServiceLinux* service = InputServiceLinux::GetInstance();

  // Allow the FileDescriptorWatcher in DeviceMonitorLinux to start watching.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service);
  std::vector<device::mojom::InputDeviceInfoPtr> devices;
  service->GetDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i)
    ASSERT_TRUE(!devices[i]->id.empty());
}

}  // namespace device
