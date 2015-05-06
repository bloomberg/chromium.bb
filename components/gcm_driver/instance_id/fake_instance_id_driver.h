// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_INSTANCE_ID_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_INSTANCE_ID_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace instance_id {

class InstanceID;

class FakeInstanceIDDriver : public InstanceIDDriver {
 public:
  FakeInstanceIDDriver();
  ~FakeInstanceIDDriver() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInstanceIDDriver);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_INSTANCE_ID_DRIVER_H_
