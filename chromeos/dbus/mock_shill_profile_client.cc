// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_shill_profile_client.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace chromeos {

MockShillProfileClient::MockShillProfileClient() {
  EXPECT_CALL(*this, Init(_)).Times(AnyNumber());
}

MockShillProfileClient::~MockShillProfileClient() {}

}  // namespace chromeos
