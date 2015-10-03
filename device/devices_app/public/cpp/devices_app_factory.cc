// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/public/cpp/devices_app_factory.h"

#include "device/devices_app/devices_app.h"

namespace device {

// static
scoped_ptr<mojo::ApplicationDelegate> DevicesAppFactory::CreateApp() {
  return scoped_ptr<mojo::ApplicationDelegate>(new DevicesApp());
}

}  // namespace device
