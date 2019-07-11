// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/files/file_enumerator.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/macros.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test_debug_listener.h"
#include "fuchsia/engine/test_devtools_list_fetcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestServerRoot[] = FILE_PATH_LITERAL("fuchsia/engine/test/data");

}  // namespace

class WebEngineDebugIntegrationTest : public testing::Test {
 public:
  WebEngineDebugIntegrationTest()
      : dev_tools_listener_binding_(&dev_tools_listener_) {}

  ~WebEngineDebugIntegrationTest() override = default;

  void SetUp() override {
    web_context_provider_ =
        base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
            ->ConnectToService<fuchsia::web::ContextProvider>();
    web_context_provider_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });

    WaitForWebEngine();

    // Connect to the Debug API.
    base::FileEnumerator file_enum(base::FilePath("/hub/c/chromium.cmx"), false,
                                   base::FileEnumerator::DIRECTORIES);
    base::FilePath chromium_path = file_enum.Next();
    ASSERT_FALSE(chromium_path.empty());

    // There should only be one instance of WebEngine in the realm.
    ASSERT_TRUE(file_enum.Next().empty());

    debug_dir_ = std::make_unique<base::fuchsia::ServiceDirectoryClient>(
        base::fuchsia::OpenDirectory(chromium_path.Append("out/debug")));
    debug_ = debug_dir_->ConnectToServiceSync<fuchsia::web::Debug>();

    // Attach the DevToolsListener. EnableDevTools has an acknowledgement
    // callback so the listener will have been added after this call returns.
    debug_->EnableDevTools(dev_tools_listener_binding_.NewBinding());

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    ASSERT_TRUE(test_server_.Start());
  }

 protected:
  void WaitForWebEngine() {
    // Create a throwaway web context to ensure the WebEngine process is
    // initialized and a Debug instance can be created. This is necessary
    // because the Debug service is not available on the debug directory until
    // after the WebEngine is fully initialized.
    fuchsia::web::CreateContextParams create_params;
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    ASSERT_TRUE(directory.is_valid());
    create_params.set_service_directory(std::move(directory));

    fuchsia::web::ContextPtr web_context;
    web_context_provider_->Create(std::move(create_params),
                                  web_context.NewRequest());
    web_context.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::Context_GetRemoteDebuggingPort_Result>
        port_receiver(run_loop.QuitClosure());
    web_context->GetRemoteDebuggingPort(
        cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
    run_loop.Run();

    // Sanity check.
    ASSERT_TRUE(port_receiver->is_err());
    ASSERT_EQ(port_receiver->err(),
              fuchsia::web::ContextError::REMOTE_DEBUGGING_PORT_NOT_OPENED);
  }

  base::MessageLoopForIO message_loop_;

  TestDebugListener dev_tools_listener_;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding_;
  std::unique_ptr<base::fuchsia::ServiceDirectoryClient> debug_dir_;
  fuchsia::web::ContextProviderPtr web_context_provider_;
  fuchsia::web::DebugSyncPtr debug_;

  base::OnceClosure on_url_fetch_complete_ack_;

  net::EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineDebugIntegrationTest);
};

// Helper struct to intiialize all data necessary for a Context to create a
// Frame and navigate it to a specific URL.
struct TestContextAndFrame {
  explicit TestContextAndFrame(fuchsia::web::ContextProvider* context_provider,
                               std::string url) {
    // Create a Context, a Frame and navigate it to |url|.
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    if (!directory.is_valid())
      return;

    fuchsia::web::CreateContextParams create_params;
    create_params.set_service_directory(std::move(directory));
    context_provider->Create(std::move(create_params), context.NewRequest());
    context->CreateFrame(frame.NewRequest());
    frame->GetNavigationController(controller.NewRequest());
    if (!cr_fuchsia::LoadUrlAndExpectResponse(
            controller.get(), fuchsia::web::LoadUrlParams(), url)) {
      ADD_FAILURE();
      context.Unbind();
      frame.Unbind();
      controller.Unbind();
      return;
    }
  }
  ~TestContextAndFrame() = default;

  fuchsia::web::ContextPtr context;
  fuchsia::web::FramePtr frame;
  fuchsia::web::NavigationControllerPtr controller;

  DISALLOW_COPY_AND_ASSIGN(TestContextAndFrame);
};

// Test the Debug service is properly started and accessible.
TEST_F(WebEngineDebugIntegrationTest, DebugService) {
  std::string url = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data(web_context_provider_.get(), url);
  ASSERT_TRUE(frame_data.context);

  // Test the debug information is correct.
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);

  base::Value devtools_list =
      GetDevToolsListFromPort(*dev_tools_listener_.debug_ports().begin());
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  base::Value* devtools_title = devtools_list.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title->is_string());
  EXPECT_EQ(devtools_title->GetString(), "title 1");

  // Unbind the context and wait for the listener to no longer have any active
  // DevTools port.
  frame_data.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(0);
}

TEST_F(WebEngineDebugIntegrationTest, MultipleDebugClients) {
  std::string url1 = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data1(web_context_provider_.get(), url1);
  ASSERT_TRUE(frame_data1.context);

  // Test the debug information is correct.
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);
  uint16_t port1 = *dev_tools_listener_.debug_ports().begin();

  base::Value devtools_list1 = GetDevToolsListFromPort(port1);
  ASSERT_TRUE(devtools_list1.is_list());
  EXPECT_EQ(devtools_list1.GetList().size(), 1u);

  base::Value* devtools_url1 = devtools_list1.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url1->is_string());
  EXPECT_EQ(devtools_url1->GetString(), url1);

  base::Value* devtools_title1 = devtools_list1.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title1->is_string());
  EXPECT_EQ(devtools_title1->GetString(), "title 1");

  // Connect a second Debug interface.
  fuchsia::web::DebugSyncPtr debug2;
  debug2 = debug_dir_->ConnectToServiceSync<fuchsia::web::Debug>();
  TestDebugListener dev_tools_listener2;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding2(
      &dev_tools_listener2);
  debug2->EnableDevTools(dev_tools_listener_binding2.NewBinding());

  // Create a second Context, a second Frame and navigate it to title2.html.
  std::string url2 = test_server_.GetURL("/title2.html").spec();
  TestContextAndFrame frame_data2(web_context_provider_.get(), url2);
  ASSERT_TRUE(frame_data2.context);

  // Ensure each DevTools listener has the right information.
  dev_tools_listener_.RunUntilNumberOfPortsIs(2u);
  dev_tools_listener2.RunUntilNumberOfPortsIs(1u);

  uint16_t port2 = *dev_tools_listener2.debug_ports().begin();
  ASSERT_NE(port1, port2);
  ASSERT_NE(dev_tools_listener_.debug_ports().find(port2),
            dev_tools_listener_.debug_ports().end());

  base::Value devtools_list2 = GetDevToolsListFromPort(port2);
  ASSERT_TRUE(devtools_list2.is_list());
  EXPECT_EQ(devtools_list2.GetList().size(), 1u);

  base::Value* devtools_url2 = devtools_list2.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url2->is_string());
  EXPECT_EQ(devtools_url2->GetString(), url2);

  base::Value* devtools_title2 = devtools_list2.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title2->is_string());
  EXPECT_EQ(devtools_title2->GetString(), "title 2");

  // Unbind the first Context, each listener should still have one open port.
  frame_data1.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);
  dev_tools_listener2.RunUntilNumberOfPortsIs(1u);

  // Unbind the second Context, no listener should have any open port.
  frame_data2.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(0);
  dev_tools_listener2.RunUntilNumberOfPortsIs(0);
}
