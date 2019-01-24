// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/sys/cpp/fidl.h>

#include "base/fuchsia/component_context.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

class WebRunnerSmokeTest : public testing::Test {
 public:
  WebRunnerSmokeTest() : run_timeout_(TestTimeouts::action_timeout()) {}
  void SetUp() final {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebRunnerSmokeTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() == "/test.html") {
      EXPECT_FALSE(test_html_requested_);
      test_html_requested_ = true;
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("<!doctype html><img src=\"/img.png\">");
      http_response->set_content_type("text/html");
      return http_response;
    } else if (absolute_url.path() == "/img.png") {
      EXPECT_FALSE(test_image_requested_);
      test_image_requested_ = true;
      // All done!
      run_loop_.Quit();
    }
    return nullptr;
  }

 protected:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  bool test_html_requested_ = false;
  bool test_image_requested_ = false;

  base::MessageLoopForIO message_loop_;

  net::EmbeddedTestServer test_server_;

  base::RunLoop run_loop_;
};

TEST_F(WebRunnerSmokeTest, RequestHtmlAndImage) {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = test_server_.GetURL("/test.html").spec();

  auto launcher = base::fuchsia::ComponentContext::GetDefault()
                      ->ConnectToServiceSync<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerSyncPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

  run_loop_.Run();

  EXPECT_TRUE(test_html_requested_);
  EXPECT_TRUE(test_image_requested_);
}

}  // anonymous namespace
