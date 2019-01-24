// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/zx/channel.h>

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_common.h"
#include "fuchsia/runners/common/web_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "fuchsia/test/promise.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ComponentErrorHandler(zx_status_t status) {
  ZX_LOG(ERROR, status) << "Component launch failed.";
  ADD_FAILURE();
}

}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest() : run_timeout_(TestTimeouts::action_timeout()) {
    // Create a new test ServiceDirectory, and a test ComponentContext
    // connected to it, for the test to use to drive the CastRunner.
    zx::channel service_directory_request, service_directory_client;
    zx_status_t status = zx::channel::create(0, &service_directory_client,
                                             &service_directory_request);
    ZX_CHECK(status == ZX_OK, status) << "zx_channel_create";

    test_service_directory_ = std::make_unique<base::fuchsia::ServiceDirectory>(
        std::move(service_directory_request));
    test_component_context_ = std::make_unique<base::fuchsia::ComponentContext>(
        std::move(service_directory_client));

    // Create the AppConfigManager.
    app_config_manager_ =
        std::make_unique<FakeApplicationConfigManager>(&test_server_);
    app_config_binding_ = std::make_unique<
        fidl::Binding<chromium::cast::ApplicationConfigManager>>(
        app_config_manager_.get());
    chromium::cast::ApplicationConfigManagerPtr app_config_manager_interface;
    app_config_binding_->Bind(app_config_manager_interface.NewRequest());

    // Create the CastRunner, published into |test_service_directory_|.
    cast_runner_ = std::make_unique<CastRunner>(
        test_service_directory_.get(),
        WebContentRunner::CreateDefaultWebContext(),
        std::move(app_config_manager_interface),
        cast_runner_run_loop_.QuitClosure());

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    cast_runner_ptr_ =
        test_component_context_->ConnectToService<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([this](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
      cast_runner_run_loop_.Quit();
    });
  }

  void SetUp() override { ASSERT_TRUE(test_server_.Start()); }

  void TearDown() override {
    // Disconnect the CastRunner.
    cast_runner_ptr_.Unbind();
    cast_runner_run_loop_.Run();
  }

 protected:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  base::MessageLoopForIO message_loop_;

  net::EmbeddedTestServer test_server_;

  std::unique_ptr<FakeApplicationConfigManager> app_config_manager_;
  std::unique_ptr<fidl::Binding<chromium::cast::ApplicationConfigManager>>
      app_config_binding_;

  // ServiceDirectory into which the CastRunner will publish itself.
  std::unique_ptr<base::fuchsia::ServiceDirectory> test_service_directory_;
  std::unique_ptr<base::fuchsia::ComponentContext> test_component_context_;

  std::unique_ptr<CastRunner> cast_runner_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;
  base::RunLoop cast_runner_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CastRunnerIntegrationTest);
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller_ptr;
  base::fuchsia::ComponentContext component_services(StartCastComponent(
      base::StringPrintf("cast:%s",
                         FakeApplicationConfigManager::kTestCastAppId),
      &cast_runner_ptr_, component_controller_ptr.NewRequest()));
  component_controller_ptr.set_error_handler(&ComponentErrorHandler);

  // Access the NavigationController from the WebComponent. The test will hang
  // here if no WebComponent was created.
  chromium::web::NavigationControllerPtr nav_controller;
  {
    base::RunLoop run_loop;
    webrunner::Promise<WebComponent*> web_component(run_loop.QuitClosure());
    cast_runner_->GetWebComponentForTest(web_component.GetReceiveCallback());
    run_loop.Run();
    ASSERT_NE(*web_component, nullptr);
    (*web_component)
        ->frame()
        ->GetNavigationController(nav_controller.NewRequest());
  }

  // Ensure the NavigationEntry has the expected URL.
  {
    base::RunLoop run_loop;
    webrunner::Promise<std::unique_ptr<chromium::web::NavigationEntry>>
        nav_entry(run_loop.QuitClosure());
    nav_controller->GetVisibleEntry(
        webrunner::ConvertToFitFunction(nav_entry.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ(nav_entry->get()->url, test_server_.base_url().spec());
  }
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller_ptr;
  base::fuchsia::ComponentContext component_services(
      StartCastComponent("cast:99999999", &cast_runner_ptr_,
                         component_controller_ptr.NewRequest()));
  component_controller_ptr.set_error_handler(&ComponentErrorHandler);

  // Ensure no WebComponent was created.
  base::RunLoop run_loop;
  webrunner::Promise<WebComponent*> web_component(run_loop.QuitClosure());
  cast_runner_->GetWebComponentForTest(web_component.GetReceiveCallback());
  run_loop.Run();
  EXPECT_EQ(*web_component, nullptr);
}
