// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_base.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/test_service/test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

typedef ShellTestBase ShellTestBaseTest;

void PingCallback(base::MessageLoop* message_loop) {
  VLOG(2) << "Ping callback";
  message_loop->QuitWhenIdle();
}

TEST_F(ShellTestBaseTest, LaunchServiceInProcess) {
  InitMojo();

  InterfacePtr<mojo::test::ITestService> test_service;

  {
    MessagePipe mp;
    test_service.Bind(mp.handle0.Pass());
    LaunchServiceInProcess(GURL("mojo:mojo_test_service"),
                           mojo::test::ITestService::Name_,
                           mp.handle1.Pass());
  }

  test_service->Ping(base::Bind(&PingCallback,
                                base::Unretained(message_loop())));
  message_loop()->Run();

  test_service.reset();

  // This will run until the test service has actually quit (which it will,
  // since we killed the only connection to it).
  message_loop()->Run();
}

}  // namespace
}  // namespace test
}  // namespace shell
}  // namespace mojo

