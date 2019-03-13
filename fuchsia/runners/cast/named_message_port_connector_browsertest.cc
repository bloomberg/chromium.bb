// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/engine/test/test_common.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

class NamedMessagePortConnectorTest
    : public cr_fuchsia::WebEngineBrowserTest,
      public chromium::web::NavigationEventObserver {
 public:
  NamedMessagePortConnectorTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~NamedMessagePortConnectorTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    frame_ = WebEngineBrowserTest::CreateFrame(this);
  }

  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    connector_.NotifyPageLoad(frame_.get());
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

  void CheckLoadUrl(const std::string& url,
                    chromium::web::NavigationController* controller) {
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    controller->LoadUrl(url, chromium::web::LoadUrlParams());
    navigate_run_loop_->Run();
    navigate_run_loop_.reset();
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  chromium::web::FramePtr frame_;
  NamedMessagePortConnector connector_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(NamedMessagePortConnectorTest);
};

IN_PROC_BROWSER_TEST_F(NamedMessagePortConnectorTest,
                       NamedMessagePortConnectorEndToEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/connector.html"));

  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  base::RunLoop receive_port_run_loop;
  cr_fuchsia::ResultReceiver<chromium::web::MessagePortPtr> message_port(
      receive_port_run_loop.QuitClosure());
  connector_.Register(
      "hello",
      base::BindRepeating(&cr_fuchsia::ResultReceiver<
                              chromium::web::MessagePortPtr>::ReceiveResult,
                          base::Unretained(&message_port)),
      frame_.get());
  CheckLoadUrl(test_url.spec(), controller.get());

  receive_port_run_loop.Run();

  chromium::web::WebMessage msg;
  msg.data = cr_fuchsia::MemBufferFromString("ping");
  cr_fuchsia::ResultReceiver<bool> post_result;
  (*message_port)
      ->PostMessage(std::move(msg), cr_fuchsia::CallbackToFitFunction(
                                        post_result.GetReceiveCallback()));

  std::vector<std::string> test_messages = {"early 1", "early 2", "ack ping"};
  for (std::string expected_msg : test_messages) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> message_receiver(
        run_loop.QuitClosure());
    (*message_port)
        ->ReceiveMessage(cr_fuchsia::CallbackToFitFunction(
            message_receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ(cr_fuchsia::StringFromMemBufferOrDie(message_receiver->data),
              expected_msg);
  }

  // Ensure that the MessagePort is dropped when navigating away.
  {
    base::RunLoop run_loop;
    (*message_port).set_error_handler([&run_loop](zx_status_t) {
      run_loop.Quit();
    });
    controller->LoadUrl("about:blank", chromium::web::LoadUrlParams());
    run_loop.Run();
  }

  connector_.Unregister(frame_.get(), "hello");
}
