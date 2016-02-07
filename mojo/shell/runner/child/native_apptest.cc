// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/runner/child/test_native_service.mojom.h"

namespace mojo {
namespace shell {
namespace {
void InvertCallback(bool* result, bool from_native) {
  *result = from_native;
}
}  // namespace

using NativeAppTest = mojo::test::ApplicationTestBase;

TEST_F(NativeAppTest, Connect) {
  test::TestNativeServicePtr native_service;
  shell()->ConnectToService("exe:mojo_runner_child_apptest_native_target",
                            &native_service);

  bool result = false;
  native_service->Invert(
      true, base::Bind(&InvertCallback, base::Unretained(&result)));
  native_service.WaitForIncomingResponse();
  EXPECT_FALSE(result);

  native_service->Invert(
      false, base::Bind(&InvertCallback, base::Unretained(&result)));
  native_service.WaitForIncomingResponse();
  EXPECT_TRUE(result);
}

}  // namespace shell
}  // namespace mojo
