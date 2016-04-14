// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <cctype>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/test_runner/mock_credential_manager_client.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/spell_check_client.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_plugin.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebLayoutAndPaintAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace test_runner {

namespace {

class LayoutAndPaintCallback : public blink::WebLayoutAndPaintAsyncCallback {
 public:
  LayoutAndPaintCallback(const base::Closure& callback)
      : callback_(callback), wait_for_popup_(false) {
  }
  virtual ~LayoutAndPaintCallback() {
  }

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }

  // WebLayoutAndPaintAsyncCallback implementation.
  void didLayoutAndPaint() override;

 private:
  base::Closure callback_;
  bool wait_for_popup_;
};

std::string DumpAllBackForwardLists(TestInterfaces* interfaces,
                                    WebTestDelegate* delegate) {
  std::string result;
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces->GetWindowList();
  for (size_t i = 0; i < window_list.size(); ++i)
    result.append(delegate->DumpHistoryForWindow(window_list.at(i)));
  return result;
}

}  // namespace

WebTestProxyBase::WebTestProxyBase()
    : test_interfaces_(nullptr),
      delegate_(nullptr),
      web_view_(nullptr),
      web_widget_(nullptr) {}

WebTestProxyBase::~WebTestProxyBase() {
  test_interfaces_->WindowClosed(this);
}

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

std::string WebTestProxyBase::DumpBackForwardLists() {
  return DumpAllBackForwardLists(test_interfaces_, delegate_);
}

void LayoutAndPaintCallback::didLayoutAndPaint() {
  TRACE_EVENT0("shell", "LayoutAndPaintCallback::didLayoutAndPaint");
  if (wait_for_popup_) {
    wait_for_popup_ = false;
    return;
  }

  if (!callback_.is_null())
    callback_.Run();
  delete this;
}

void WebTestProxyBase::LayoutAndPaintAsyncThen(const base::Closure& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::LayoutAndPaintAsyncThen");

  LayoutAndPaintCallback* layout_and_paint_callback =
      new LayoutAndPaintCallback(callback);
  web_widget_->layoutAndPaintAsync(layout_and_paint_callback);
  if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
    layout_and_paint_callback->set_wait_for_popup(true);
    popup->layoutAndPaintAsync(layout_and_paint_callback);
  }
}

void WebTestProxyBase::GetScreenOrientationForTesting(
    blink::WebScreenInfo& screen_info) {
  MockScreenOrientationClient* mock_client =
      test_interfaces_->GetTestRunner()->getMockScreenOrientationClient();
  if (mock_client->IsDisabled())
    return;
  // Override screen orientation information with mock data.
  screen_info.orientationType = mock_client->CurrentOrientationType();
  screen_info.orientationAngle = mock_client->CurrentOrientationAngle();
}

}  // namespace test_runner
