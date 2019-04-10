// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUnimplementedLogMessage[] =
    "Unimplemented stub function called: cast.__platform__.";

constexpr char kStubBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/not_implemented_api_bindings.js");

class StubBindingsTest : public cr_fuchsia::WebEngineBrowserTest,
                         public chromium::web::NavigationEventObserver {
 public:
  StubBindingsTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~StubBindingsTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(embedded_test_server()->Start());

    frame_ = WebEngineBrowserTest::CreateFrame(this);
    FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame_);
    frame_impl->set_javascript_console_message_hook_for_test(
        base::BindRepeating(&StubBindingsTest::OnLogMessage,
                            base::Unretained(this)));
    frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);

    base::FilePath stub_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &stub_path));
    stub_path = stub_path.AppendASCII(kStubBindingsPath);
    DCHECK(base::PathExists(stub_path));
    fuchsia::mem::Buffer stub_buf = cr_fuchsia::MemBufferFromFile(
        base::File(stub_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
    CHECK(stub_buf.vmo);
    frame_->AddJavaScriptBindings(
        static_cast<uint64_t>(CastPlatformBindingsId::NOT_IMPLEMENTED_API),
        {"*"}, std::move(stub_buf),
        [](bool result) { CHECK(result) << "Couldn't inject stub bindings."; });

    chromium::web::NavigationControllerPtr controller;
    frame_->GetNavigationController(controller.NewRequest());
    const GURL page_url(embedded_test_server()->GetURL("/defaultresponse"));
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    controller->LoadUrl(page_url.spec(), chromium::web::LoadUrlParams());
    navigate_run_loop_->Run();
    navigate_run_loop_.reset();
  }

  void OnLogMessage(base::StringPiece message) {
    captured_logs_.push_back(message.as_string());

    if (on_log_message_received_cb_)
      std::move(on_log_message_received_cb_).Run();
  }

  // Processes tasks from the the message loop until the captured JavaScript log
  // contains |message|.
  void WaitForLogMessage(base::StringPiece expected_message) {
    while (true) {
      base::RunLoop run_loop;
      on_log_message_received_cb_ = run_loop.QuitClosure();
      run_loop.Run();

      if (std::find_if(captured_logs_.begin(), captured_logs_.end(),
                       [expected_message = expected_message.as_string()](
                           const std::string& cur_string) {
                         return cur_string.find(expected_message) !=
                                std::string::npos;
                       }) != captured_logs_.end()) {
        captured_logs_.clear();
        return;
      }
    }
  }

  // chromium::web::NavigationEventObserver implementation.
  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  chromium::web::FramePtr frame_;
  base::OnceClosure on_log_message_received_cb_;

  // Stores accumulated log messages.
  std::vector<std::string> captured_logs_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(StubBindingsTest);
};

struct StubExpectation {
  std::string function_name;
  std::string result = "undefined";
};

IN_PROC_BROWSER_TEST_F(StubBindingsTest, ApiCoverage) {
  // A list of APIs to exercise, along with the JSON representation of their
  // expected output.
  const std::vector<StubExpectation> kExpectations = {
      {"canDisplayType", "true"},
      {"setTouchInputSupport", "promise {\"displayControls\":true}"},
      {"setAssistantMessageHandler"},
      {"sendAssistantRequest"},
      {"setWindowRequestHandler"},
      {"takeScreenshot"},

      {"crypto.encrypt", "promise {}"},
      {"crypto.decrypt", "promise {}"},
      {"crypto.sign", "promise {}"},
      {"crypto.unwrapKey", "rejected promise undefined"},
      {"crypto.verify", "promise true"},
      {"crypto.wrapKey", "rejected promise undefined"},

      {"cryptokeys.getKeyByName", "rejected promise \"\""},

      {"display.updateOutputMode", "promise {}"},
      {"display.getHdcpVersion", "promise \"0\""},

      {"metrics.logBoolToUma"},
      {"metrics.logIntToUma"},
      {"metrics.logEventToUma"},
      {"metrics.logHistogramValueToUma"},
      {"metrics.setMplVersion"},

      {"accessibility.getAccessibilitySettings",
       "promise {\"isColorInversionEnabled\":false,"
       "\"isScreenReaderEnabled\":false}"},
      {"accessibility.setColorInversion"},
      {"accessibility.setMagnificationGesture"},

      {"windowManager.onBackGesture", "via callback undefined"},
      {"windowManager.onBackGestureProgress", "via callback [0]"},
      {"windowManager.onBackGestureCancel", "via callback undefined"},
      {"windowManager.onTopDragGestureDone", "via callback undefined"},
      {"windowManager.onTopDragGestureProgress", "via callback [0]"},
      {"windowManager.onTapGesture", "via callback undefined"},
      {"windowManager.onTapDownGesture", "via callback undefined"},
      {"windowManager.canTopDrag", "false"},
      {"windowManager.canGoBack", "false"},
      {"windowManager.onRightDragGestureDone", "via callback undefined"},
      {"windowManager.onRightDragGestureProgress", "via callback [0,0]"},
      {"windowManager.canRightDrag", "false"},
  };

  for (const auto& expectation : kExpectations) {
    frame_->ExecuteJavaScript(
        {"*"},
        cr_fuchsia::MemBufferFromString(
            base::StringPrintf("try { cast.__platform__.%s(); } catch {}",
                               expectation.function_name.c_str())),
        chromium::web::ExecuteMode::IMMEDIATE_ONCE,
        [](bool success) { EXPECT_TRUE(success); });

    WaitForLogMessage(base::StringPrintf("%s%s(", kUnimplementedLogMessage,
                                         expectation.function_name.c_str()));
    WaitForLogMessage(
        base::StringPrintf("Returning %s", expectation.result.c_str()));
  }
}

IN_PROC_BROWSER_TEST_F(StubBindingsTest, FunctionArgumentsInLogMessage) {
  frame_->ExecuteJavaScript(
      {"*"},
      cr_fuchsia::MemBufferFromString(
          "cast.__platform__.sendAssistantRequest(1,2,'foo');"),
      chromium::web::ExecuteMode::IMMEDIATE_ONCE,
      [](bool success) { EXPECT_TRUE(success); });

  WaitForLogMessage(base::StringPrintf("%ssendAssistantRequest(1, 2, \"foo\")",
                                       kUnimplementedLogMessage));
}

}  // namespace
