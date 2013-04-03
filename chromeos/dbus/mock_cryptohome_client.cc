// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_cryptohome_client.h"

#include "base/bind.h"
#include "base/message_loop.h"

using ::testing::Invoke;
using ::testing::_;

namespace chromeos {

namespace {

// Runs callback with true.
void RunCallbackWithTrue(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

}  // namespace

MockCryptohomeClient::MockCryptohomeClient() {
  ON_CALL(*this, IsMounted(_))
      .WillByDefault(Invoke(&RunCallbackWithTrue));
  ON_CALL(*this, InstallAttributesIsReady(_))
      .WillByDefault(Invoke(&RunCallbackWithTrue));
}

MockCryptohomeClient::~MockCryptohomeClient() {}

}  // namespace chromeos
