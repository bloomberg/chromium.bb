// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/background/background_shell.h"

#include "base/run_loop.h"
#include "mojo/shell/background/tests/test.mojom.h"
#include "mojo/shell/background/tests/test_catalog_store.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

const char kTestName[] = "mojo:test-app";

class ShellClientImpl : public ShellClient {
 public:
  ShellClientImpl() {}
  ~ShellClientImpl() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellClientImpl);
};

scoped_ptr<TestCatalogStore> BuildTestCatalogStore() {
  scoped_ptr<base::ListValue> apps(new base::ListValue);
  apps->Append(BuildPermissiveSerializedAppInfo(kTestName, "test"));
  return make_scoped_ptr(new TestCatalogStore(std::move(apps)));
}

}  // namespace

// Uses BackgroundShell to start the shell in the background and connects to
// background_shell_test_app, verifying we can send a message to the app.
// An ApplicationCatalogStore is supplied to avoid using a manifest.
#if defined(OS_ANDROID)
// TODO(crbug.com/589784): This test is disabled, as it fails
// on the Android GN bot.
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST(BackgroundShellTest, MAYBE_Basic) {
  base::MessageLoop message_loop;
  BackgroundShell background_shell;
  scoped_ptr<BackgroundShell::InitParams> init_params(
      new BackgroundShell::InitParams);
  scoped_ptr<TestCatalogStore> store_ptr = BuildTestCatalogStore();
  TestCatalogStore* store = store_ptr.get();
  init_params->catalog_store = std::move(store_ptr);
  background_shell.Init(std::move(init_params));
  ShellClientImpl shell_client;
  ShellConnection shell_connection(
      &shell_client, background_shell.CreateShellClientRequest(kTestName));
  mojom::TestServicePtr test_service;
  shell_connection.connector()->ConnectToInterface(
      "mojo:background_shell_test_app", &test_service);
  base::RunLoop run_loop;
  bool got_result = false;
  test_service->Test([&run_loop, &got_result]() {
    got_result = true;
    run_loop.Quit();
  });
  run_loop.Run();
  EXPECT_TRUE(got_result);
  EXPECT_TRUE(store->get_store_called());
}

}  // namespace shell
}  // namespace mojo
