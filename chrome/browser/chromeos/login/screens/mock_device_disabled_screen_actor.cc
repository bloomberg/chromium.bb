// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_actor.h"

using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::NotNull;

namespace chromeos {

MockDeviceDisabledScreenActor::MockDeviceDisabledScreenActor()
    : delegate_(nullptr) {
  EXPECT_CALL(*this, MockSetDelegate(NotNull())).Times(AtLeast(1));
  EXPECT_CALL(*this, MockSetDelegate(nullptr)).Times(AtMost(1));
}

MockDeviceDisabledScreenActor::~MockDeviceDisabledScreenActor() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void MockDeviceDisabledScreenActor::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

}  // namespace chromeos
