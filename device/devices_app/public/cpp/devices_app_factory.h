// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_
#define DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace mojo {
class ShellClient;
}

namespace device {

// Public factory for creating new instances of the devices app.
class DevicesAppFactory {
 public:
  // Creates a DevicesApp delegate which can be used to launch a new instance
  // of the devices app on a mojo application runner. The caller owns the
  // delegate.
  static scoped_ptr<mojo::ShellClient> CreateApp();
};

}  // namespace device

#endif  // DEVICE_DEVICES_APP_PUBLIC_CPP_DEVICES_APP_FACTORY_H_
