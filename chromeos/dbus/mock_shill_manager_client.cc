// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_shill_manager_client.h"

#include "dbus/object_path.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace chromeos {

MockShillManagerClient::MockShillManagerClient() {
  EXPECT_CALL(*this, Init(_)).Times(AnyNumber());
}

MockShillManagerClient::~MockShillManagerClient() {}

}  // namespace chromeos
