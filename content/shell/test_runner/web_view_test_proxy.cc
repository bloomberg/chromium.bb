// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/shell/test_runner/accessibility_controller.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/text_input_controller.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace test_runner {

WebViewTestProxyBase::WebViewTestProxyBase()
    : test_interfaces_(nullptr),
      delegate_(nullptr),
      web_view_(nullptr),
      accessibility_controller_(new AccessibilityController(this)),
      text_input_controller_(new TextInputController(this)),
      view_test_runner_(new TestRunnerForSpecificView(this)) {
  WebWidgetTestProxyBase::set_web_view_test_proxy_base(this);
}

WebViewTestProxyBase::~WebViewTestProxyBase() {
  test_interfaces_->WindowClosed(this);
}

void WebViewTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

void WebViewTestProxyBase::Reset() {
  accessibility_controller_->Reset();
  // text_input_controller_ doesn't have any state to reset.
  view_test_runner_->Reset();
  WebWidgetTestProxyBase::Reset();

  for (blink::WebFrame* frame = web_view_->mainFrame(); frame;
       frame = frame->traverseNext()) {
    if (frame->isWebLocalFrame())
      delegate_->GetWebWidgetTestProxyBase(frame->toWebLocalFrame())->Reset();
  }
}

void WebViewTestProxyBase::BindTo(blink::WebLocalFrame* frame) {
  accessibility_controller_->Install(frame);
  text_input_controller_->Install(frame);
  view_test_runner_->Install(frame);
}

}  // namespace test_runner
