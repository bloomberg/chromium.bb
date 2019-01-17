// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromium/cast/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "fuchsia/app/cast/application_config_manager/application_config_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace castrunner {

namespace {

class ApplicationConfigManagerTest : public ::testing::Test {
 public:
  ApplicationConfigManagerTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        binding_(&app_config_manager_) {}

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

  void SetUp() override {
    // Bind the service with the client-side interface.
    binding_.Bind(app_config_manager_interface_.NewRequest());
  }

  void TearDown() override {
    // Disconnect the client and wait for the service to shut down.
    base::RunLoop run_loop;
    binding_.set_error_handler([&run_loop](zx_status_t status) {
      ASSERT_EQ(status, ZX_ERR_PEER_CLOSED);
      run_loop.Quit();
    });
    app_config_manager_interface_.Unbind();
    run_loop.Run();
    binding_.set_error_handler(nullptr);
  }

  chromium::cast::ApplicationConfigManagerPtr& app_config_manager() {
    return app_config_manager_interface_;
  }

 private:
  castrunner::ApplicationConfigManager app_config_manager_;
  chromium::cast::ApplicationConfigManagerPtr app_config_manager_interface_;
  fidl::Binding<chromium::cast::ApplicationConfigManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationConfigManagerTest);
};

}  // namespace

// Tests a basic request returns the expected data.
TEST_F(ApplicationConfigManagerTest, BasicTestRequest) {
  constexpr char kTestCastAppId[] = "00000000";

  base::RunLoop run_loop;
  app_config_manager()->GetConfig(
      kTestCastAppId,
      [&run_loop, &kTestCastAppId](
          std::unique_ptr<chromium::cast::ApplicationConfig> app_config) {
        ASSERT_NE(app_config.get(), nullptr);
        EXPECT_EQ(app_config->id, kTestCastAppId);
        EXPECT_EQ(app_config->display_name, "Dummy test app");
        EXPECT_EQ(app_config->web_url, "https://www.google.com");
        run_loop.Quit();
      });
  run_loop.Run();
}

// Tests an unknown app ID returns an emoty app config.
TEST_F(ApplicationConfigManagerTest, UnknownAppId) {
  base::RunLoop run_loop;
  app_config_manager()->GetConfig(
      "ffffffff",
      [&run_loop](
          std::unique_ptr<chromium::cast::ApplicationConfig> app_config) {
        EXPECT_EQ(app_config.get(), nullptr);
        run_loop.Quit();
      });
  run_loop.Run();
}

}  // namespace castrunner