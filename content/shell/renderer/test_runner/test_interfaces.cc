// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/test_interfaces.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/test_runner/accessibility_controller.h"
#include "content/shell/renderer/test_runner/event_sender.h"
#include "content/shell/renderer/test_runner/gamepad_controller.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/text_input_controller.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

TestInterfaces::TestInterfaces()
    : accessibility_controller_(new AccessibilityController()),
      event_sender_(new EventSender(this)),
      text_input_controller_(new TextInputController()),
      test_runner_(new TestRunner(this)),
      delegate_(0) {
  blink::setLayoutTestMode(true);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing))
    blink::setFontAntialiasingEnabledForTest(true);

  // NOTE: please don't put feature specific enable flags here,
  // instead add them to RuntimeEnabledFeatures.in

  ResetAll();
}

TestInterfaces::~TestInterfaces() {
  accessibility_controller_->SetWebView(0);
  event_sender_->SetWebView(0);
  // gamepad_controller_ doesn't depend on WebView.
  text_input_controller_->SetWebView(NULL);
  test_runner_->SetWebView(0, 0);

  accessibility_controller_->SetDelegate(0);
  event_sender_->SetDelegate(0);
  // gamepad_controller_ ignores SetDelegate(0)
  // text_input_controller_ doesn't depend on WebTestDelegate.
  test_runner_->SetDelegate(0);
}

void TestInterfaces::SetWebView(blink::WebView* web_view,
                                WebTestProxyBase* proxy) {
  proxy_ = proxy;
  accessibility_controller_->SetWebView(web_view);
  event_sender_->SetWebView(web_view);
  // gamepad_controller_ doesn't depend on WebView.
  text_input_controller_->SetWebView(web_view);
  test_runner_->SetWebView(web_view, proxy);
}

void TestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  accessibility_controller_->SetDelegate(delegate);
  event_sender_->SetDelegate(delegate);
  gamepad_controller_ = GamepadController::Create(delegate);
  // text_input_controller_ doesn't depend on WebTestDelegate.
  test_runner_->SetDelegate(delegate);
  delegate_ = delegate;
}

void TestInterfaces::BindTo(blink::WebFrame* frame) {
  accessibility_controller_->Install(frame);
  event_sender_->Install(frame);
  if (gamepad_controller_)
    gamepad_controller_->Install(frame);
  text_input_controller_->Install(frame);
  test_runner_->Install(frame);
}

void TestInterfaces::ResetTestHelperControllers() {
  accessibility_controller_->Reset();
  event_sender_->Reset();
  if (gamepad_controller_)
    gamepad_controller_->Reset();
  // text_input_controller_ doesn't have any state to reset.
  blink::WebCache::clear();
}

void TestInterfaces::ResetAll() {
  ResetTestHelperControllers();
  test_runner_->Reset();
}

void TestInterfaces::SetTestIsRunning(bool running) {
  test_runner_->SetTestIsRunning(running);
}

void TestInterfaces::ConfigureForTestWithURL(const blink::WebURL& test_url,
                                             bool generate_pixels) {
  std::string spec = GURL(test_url).spec();
  test_runner_->setShouldGeneratePixelResults(generate_pixels);
  if (spec.find("loading/") != std::string::npos)
    test_runner_->setShouldDumpFrameLoadCallbacks(true);
  if (spec.find("/dumpAsText/") != std::string::npos) {
    test_runner_->setShouldDumpAsText(true);
    test_runner_->setShouldGeneratePixelResults(false);
  }
  if (spec.find("/inspector/") != std::string::npos ||
      spec.find("/inspector-enabled/") != std::string::npos)
    test_runner_->clearDevToolsLocalStorage();
  if (spec.find("/inspector/") != std::string::npos) {
    // Subfolder name determines default panel to open.
    std::string settings = "";
    std::string test_path = spec.substr(spec.find("/inspector/") + 11);
    size_t slash_index = test_path.find("/");
    if (slash_index != std::string::npos) {
      settings = base::StringPrintf("{\"lastActivePanel\":\"\\\"%s\\\"\"}",
                                    test_path.substr(0, slash_index).c_str());
    }
    test_runner_->showDevTools(settings, std::string());
  }
  if (spec.find("/viewsource/") != std::string::npos) {
    test_runner_->setShouldEnableViewSource(true);
    test_runner_->setShouldGeneratePixelResults(false);
    test_runner_->setShouldDumpAsMarkup(true);
  }
}

void TestInterfaces::WindowOpened(WebTestProxyBase* proxy) {
  window_list_.push_back(proxy);
}

void TestInterfaces::WindowClosed(WebTestProxyBase* proxy) {
  std::vector<WebTestProxyBase*>::iterator pos =
      std::find(window_list_.begin(), window_list_.end(), proxy);
  if (pos == window_list_.end()) {
    NOTREACHED();
    return;
  }
  window_list_.erase(pos);
}

AccessibilityController* TestInterfaces::GetAccessibilityController() {
  return accessibility_controller_.get();
}

EventSender* TestInterfaces::GetEventSender() {
  return event_sender_.get();
}

TestRunner* TestInterfaces::GetTestRunner() {
  return test_runner_.get();
}

WebTestDelegate* TestInterfaces::GetDelegate() {
  return delegate_;
}

WebTestProxyBase* TestInterfaces::GetProxy() {
  return proxy_;
}

const std::vector<WebTestProxyBase*>& TestInterfaces::GetWindowList() {
  return window_list_;
}

blink::WebThemeEngine* TestInterfaces::GetThemeEngine() {
  if (!test_runner_->UseMockTheme())
    return 0;
#if defined(OS_MACOSX)
  if (!theme_engine_.get())
    theme_engine_.reset(new MockWebThemeEngineMac());
#else
  if (!theme_engine_.get())
    theme_engine_.reset(new MockWebThemeEngine());
#endif
  return theme_engine_.get();
}

}  // namespace content
