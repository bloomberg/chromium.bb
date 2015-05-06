// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/fake_instance_id_driver.h"

namespace instance_id {

FakeInstanceIDDriver::FakeInstanceIDDriver()
    : InstanceIDDriver(NULL) {
}

FakeInstanceIDDriver::~FakeInstanceIDDriver() {
}

}  // namespace instance_id
