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
#include "base/threading/thread_restrictions.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/engine/test/test_common.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/cast_channel_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace {

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

class CastChannelBindingsTest : public cr_fuchsia::WebEngineBrowserTest,
                                public chromium::web::NavigationEventObserver,
                                public chromium::cast::CastChannel {
 public:
  CastChannelBindingsTest()
      : receiver_binding_(this),
        run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~CastChannelBindingsTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    frame_ = WebEngineBrowserTest::CreateFrame(this);
    connector_ = std::make_unique<NamedMessagePortConnector>();
  }

  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    connector_->NotifyPageLoad(frame_.get());
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

  void OnOpened(fidl::InterfaceHandle<chromium::web::MessagePort> channel,
                OnOpenedCallback receive_next_channel_cb) override {
    connected_channel_ = channel.Bind();
    receive_next_channel_cb_ = std::move(receive_next_channel_cb);

    if (on_channel_connected_cb_)
      std::move(on_channel_connected_cb_).Run();
  }

  void SignalReadyForNewChannel() { receive_next_channel_cb_(); }

  void WaitUntilCastChannelOpened() {
    if (connected_channel_)
      return;

    base::RunLoop run_loop;
    on_channel_connected_cb_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitUntilCastChannelClosed() {
    if (!connected_channel_)
      return;

    base::RunLoop run_loop;
    connected_channel_.set_error_handler(
        [quit_closure = run_loop.QuitClosure()](zx_status_t) {
          quit_closure.Run();
        });
    run_loop.Run();
  }

  std::string ReadStringFromChannel() {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> message(
        run_loop.QuitClosure());
    connected_channel_->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(message.GetReceiveCallback()));
    run_loop.Run();
    return cr_fuchsia::StringFromMemBufferOrDie(message->data);
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
  std::unique_ptr<NamedMessagePortConnector> connector_;
  fidl::Binding<chromium::cast::CastChannel> receiver_binding_;

  // The connected Cast Channel.
  chromium::web::MessagePortPtr connected_channel_;

  // A pending on-connect callback, to be invoked once a Cast Channel is
  // received.
  base::OnceClosure on_channel_connected_cb_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;
  OnOpenedCallback receive_next_channel_cb_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelBindingsTest);
};

IN_PROC_BROWSER_TEST_F(CastChannelBindingsTest, CastChannelBufferedInput) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel.html"));

  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  testing::InSequence seq;
  CastChannelBindings cast_channel_instance(
      frame_.get(), connector_.get(), receiver_binding_.NewBinding().Bind(),
      base::MakeExpectedNotRunClosure(FROM_HERE));

  // Verify that CastChannelBindings can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  CheckLoadUrl(test_url.spec(), controller.get());

  WaitUntilCastChannelOpened();

  auto expected_list = {"this", "is", "a", "test"};
  for (const std::string& expected : expected_list) {
    EXPECT_EQ(ReadStringFromChannel(), expected);
  }
}

IN_PROC_BROWSER_TEST_F(CastChannelBindingsTest, CastChannelReconnect) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel_reconnect.html"));
  GURL empty_url(embedded_test_server()->GetURL("/defaultresponse"));

  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  testing::InSequence seq;
  CastChannelBindings cast_channel_instance(
      frame_.get(), connector_.get(), receiver_binding_.NewBinding().Bind(),
      base::MakeExpectedNotRunClosure(FROM_HERE));

  // Verify that CastChannelBindings can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  // Also verify that the cast channel is used across inter-page navigations.
  for (int i = 0; i < 5; ++i) {
    CheckLoadUrl(test_url.spec(), controller.get());

    WaitUntilCastChannelOpened();

    WaitUntilCastChannelClosed();

    SignalReadyForNewChannel();

    WaitUntilCastChannelOpened();

    EXPECT_EQ("reconnected", ReadStringFromChannel());

    // Send a message to the channel.
    {
      chromium::web::WebMessage message;
      message.data = cr_fuchsia::MemBufferFromString("hello");

      base::RunLoop run_loop;
      cr_fuchsia::ResultReceiver<bool> post_result(run_loop.QuitClosure());
      connected_channel_->PostMessage(
          std::move(message),
          cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
      run_loop.Run();
      EXPECT_EQ(true, *post_result);
    }

    EXPECT_EQ("ack hello", ReadStringFromChannel());

    // Navigate away.
    CheckLoadUrl(empty_url.spec(), controller.get());

    SignalReadyForNewChannel();
  }
}

}  // namespace
