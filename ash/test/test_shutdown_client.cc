// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shutdown_client.h"

namespace ash {
namespace test {

TestShutdownClient::TestShutdownClient() : num_shutdown_requests_(0) {}

TestShutdownClient::~TestShutdownClient() {}

void TestShutdownClient::RequestShutdown() {
  ++num_shutdown_requests_;
}

}  // namespace test
}  // namespace ash
