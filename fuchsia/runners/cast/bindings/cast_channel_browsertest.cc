// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "fuchsia/common/mem_buffer_util.h"
#include "fuchsia/common/named_message_port_connector.h"
#include "fuchsia/common/test/test_common.h"
#include "fuchsia/common/test/webrunner_browser_test.h"
#include "fuchsia/runners/cast/bindings/cast_channel.h"
#include "fuchsia/test/promise.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

class CastChannelImplTest : public webrunner::WebRunnerBrowserTest,
                            public chromium::web::NavigationEventObserver {
 public:
  CastChannelImplTest() : run_timeout_(TestTimeouts::action_timeout()) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~CastChannelImplTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    webrunner::WebRunnerBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    frame_ = WebRunnerBrowserTest::CreateFrame(this);
    connector_ = std::make_unique<webrunner::NamedMessagePortConnector>();
  }

  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    connector_->NotifyPageLoad(frame_.get());
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

  void CheckLoadUrl(const std::string& url,
                    chromium::web::NavigationController* controller) {
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    controller->LoadUrl(url, nullptr);
    navigate_run_loop_->Run();
    navigate_run_loop_.reset();
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  chromium::web::FramePtr frame_;
  std::unique_ptr<webrunner::NamedMessagePortConnector> connector_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelImplTest);
};

IN_PROC_BROWSER_TEST_F(CastChannelImplTest, CastChannelBuffered) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel.html"));

  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  testing::InSequence seq;
  CastChannelImpl cast_channel_instance(frame_.get(), connector_.get());
  fidl::Binding<chromium::cast::CastChannel> cast_channel_binding(
      &cast_channel_instance);
  chromium::cast::CastChannelPtr cast_channel =
      cast_channel_binding.NewBinding().Bind();

  // Verify that CastChannelImpl can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  CheckLoadUrl(test_url.spec(), controller.get());

  chromium::web::MessagePortPtr channel;

  // Expect channel open.
  {
    base::RunLoop run_loop;
    webrunner::Promise<fidl::InterfaceHandle<chromium::web::MessagePort>>
        channel_promise(run_loop.QuitClosure());
    cast_channel->Connect(
        webrunner::ConvertToFitFunction(channel_promise.GetReceiveCallback()));
    run_loop.Run();

    channel = channel_promise->Bind();
  }

  // Send a message to the channel.
  auto expected_list = {"this", "is", "a", "test"};
  for (const std::string& expected : expected_list) {
    base::RunLoop run_loop;
    webrunner::Promise<chromium::web::WebMessage> message(
        run_loop.QuitClosure());
    channel->ReceiveMessage(
        webrunner::ConvertToFitFunction(message.GetReceiveCallback()));
    run_loop.Run();

    EXPECT_EQ(webrunner::StringFromMemBufferOrDie(message->data), expected);
  }
}

IN_PROC_BROWSER_TEST_F(CastChannelImplTest, CastChannelReconnect) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel_reconnect.html"));
  GURL empty_url(embedded_test_server()->GetURL("/defaultresponse"));

  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  testing::InSequence seq;
  CastChannelImpl cast_channel_instance(frame_.get(), connector_.get());
  fidl::Binding<chromium::cast::CastChannel> cast_channel_binding(
      &cast_channel_instance);
  chromium::cast::CastChannelPtr cast_channel =
      cast_channel_binding.NewBinding().Bind();

  // Verify that CastChannelImpl can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  // Also verify that the cast channel is used across inter-page navigations.
  for (int i = 0; i < 5; ++i) {
    CheckLoadUrl(test_url.spec(), controller.get());

    chromium::web::MessagePortPtr channel;

    // Expect channel open.
    {
      base::RunLoop run_loop;
      webrunner::Promise<fidl::InterfaceHandle<chromium::web::MessagePort>>
          channel_promise(run_loop.QuitClosure());
      cast_channel->Connect(webrunner::ConvertToFitFunction(
          channel_promise.GetReceiveCallback()));
      run_loop.Run();

      channel = channel_promise->Bind();
    }

    // Expect channel close.
    {
      base::RunLoop run_loop;
      channel.set_error_handler([&run_loop](zx_status_t) { run_loop.Quit(); });
      run_loop.Run();
    }

    // Expect channel re-open.
    {
      base::RunLoop run_loop;
      webrunner::Promise<fidl::InterfaceHandle<chromium::web::MessagePort>>
          channel_promise(run_loop.QuitClosure());
      cast_channel->Connect(webrunner::ConvertToFitFunction(
          channel_promise.GetReceiveCallback()));
      run_loop.Run();

      channel = channel_promise->Bind();
    }

    // Read "reconnected" from the channel.
    {
      base::RunLoop run_loop;
      webrunner::Promise<chromium::web::WebMessage> message(
          run_loop.QuitClosure());
      channel->ReceiveMessage(
          webrunner::ConvertToFitFunction(message.GetReceiveCallback()));
      run_loop.Run();

      EXPECT_EQ(webrunner::StringFromMemBufferOrDie(message->data),
                "reconnected");
    }

    // Send a message to the channel.
    {
      chromium::web::WebMessage message;
      message.data = webrunner::MemBufferFromString("hello");

      base::RunLoop run_loop;
      webrunner::Promise<bool> post_result(run_loop.QuitClosure());
      channel->PostMessage(
          std::move(message),
          webrunner::ConvertToFitFunction(post_result.GetReceiveCallback()));
      run_loop.Run();
      EXPECT_EQ(true, *post_result);
    }

    // Get a message from the channel.
    {
      base::RunLoop run_loop;
      webrunner::Promise<chromium::web::WebMessage> message(
          run_loop.QuitClosure());
      channel->ReceiveMessage(
          webrunner::ConvertToFitFunction(message.GetReceiveCallback()));
      run_loop.Run();

      EXPECT_EQ(webrunner::StringFromMemBufferOrDie(message->data),
                "ack hello");
    }

    // Navigate away.
    CheckLoadUrl(empty_url.spec(), controller.get());
  }
}
