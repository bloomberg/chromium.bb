// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_
#define DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {
class ApplicationDelegate;
}

namespace device {

// Public factory for creating new instances of the devices app.
class DevicesAppFactory {
 public:
  // Creates a DevicesApp delegate which can be used to launch a new instance
  // of the devices app on a mojo application runner. The caller owns the
  // delegate.
  //
  // |service_task_runner| is the thread TaskRunner on which the UsbService
  // lives. This argument should be removed once UsbService is owned by the
  // USB device manager and no longer part of the public device API. If null,
  // the app will construct its own DeviceClient and UsbService.
  static scoped_ptr<mojo::ApplicationDelegate> CreateApp(
      scoped_refptr<base::SequencedTaskRunner> service_task_runner);
};

}  // namespace device

#endif  // DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_
