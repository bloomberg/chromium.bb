// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHUTDOWN_CLIENT_H_
#define ASH_TEST_TEST_SHUTDOWN_CLIENT_H_

#include "ash/public/interfaces/shutdown.mojom.h"
#include "base/macros.h"

namespace ash {
namespace test {

// Fake implementation of ShutdownClient that just logs requests to lock the
// screen and shut down the device.
class TestShutdownClient : public mojom::ShutdownClient {
 public:
  TestShutdownClient();
  ~TestShutdownClient() override;

  int num_shutdown_requests() const { return num_shutdown_requests_; }

  // LockStateControllerDelegate implementation.
  void RequestShutdown() override;

 private:
  int num_shutdown_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestShutdownClient);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHUTDOWN_CLIENT_H_
