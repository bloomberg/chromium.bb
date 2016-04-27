// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/test_interfaces.h"

#include <stddef.h>

#include <string>

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/test_runner/app_banner_client.h"
#include "components/test_runner/gamepad_controller.h"
#include "components/test_runner/gc_controller.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/text_input_controller.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace test_runner {

TestInterfaces::TestInterfaces()
    : test_runner_(new TestRunner(this)),
      delegate_(nullptr),
      app_banner_client_(nullptr) {
  blink::setLayoutTestMode(true);
  // NOTE: please don't put feature specific enable flags here,
  // instead add them to RuntimeEnabledFeatures.in

  ResetAll();
}

TestInterfaces::~TestInterfaces() {
  // gamepad_controller_ doesn't depend on WebView.
  test_runner_->SetWebView(nullptr);

  // gamepad_controller_ ignores SetDelegate(nullptr)
  test_runner_->SetDelegate(nullptr);
}

void TestInterfaces::SetWebView(blink::WebView* web_view,
                                WebTestProxyBase* proxy) {
  // gamepad_controller_ doesn't depend on WebView.
  test_runner_->SetWebView(web_view);
}

void TestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  gamepad_controller_ = GamepadController::Create(delegate);
  test_runner_->SetDelegate(delegate);
  delegate_ = delegate;
}

void TestInterfaces::BindTo(blink::WebFrame* frame) {
  if (gamepad_controller_)
    gamepad_controller_->Install(frame);
  GCController::Install(frame);
}

void TestInterfaces::ResetTestHelperControllers() {
  if (gamepad_controller_)
    gamepad_controller_->Reset();
  blink::WebCache::clear();

  for (WebTestProxyBase* web_test_proxy_base : window_list_)
    web_test_proxy_base->Reset();
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
  size_t path_start = spec.rfind("LayoutTests/");
  if (path_start != std::string::npos)
    spec = spec.substr(path_start);
  test_runner_->setShouldGeneratePixelResults(generate_pixels);
  if (spec.find("loading/") != std::string::npos)
    test_runner_->setShouldDumpFrameLoadCallbacks(true);
  if (spec.find("/dumpAsText/") != std::string::npos) {
    test_runner_->setShouldDumpAsText(true);
    test_runner_->setShouldGeneratePixelResults(false);
  }
  if (spec.find("/inspector/") != std::string::npos ||
      spec.find("/inspector-enabled/") != std::string::npos)
    test_runner_->ClearDevToolsLocalStorage();
  if (spec.find("/inspector/") != std::string::npos) {
    // Subfolder name determines default panel to open.
    std::string test_path = spec.substr(spec.find("/inspector/") + 11);
    base::DictionaryValue settings;
    settings.SetString("testPath", base::GetQuotedJSONString(spec));
    std::string settings_string;
    base::JSONWriter::Write(settings, &settings_string);
    test_runner_->ShowDevTools(settings_string, std::string());
  }
  if (spec.find("/viewsource/") != std::string::npos) {
    test_runner_->setShouldEnableViewSource(true);
    test_runner_->setShouldGeneratePixelResults(false);
    test_runner_->setShouldDumpAsMarkup(true);
  }
}

void TestInterfaces::SetAppBannerClient(AppBannerClient* app_banner_client) {
  app_banner_client_ = app_banner_client;
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

TestRunner* TestInterfaces::GetTestRunner() {
  return test_runner_.get();
}

WebTestDelegate* TestInterfaces::GetDelegate() {
  return delegate_;
}

const std::vector<WebTestProxyBase*>& TestInterfaces::GetWindowList() {
  return window_list_;
}

blink::WebThemeEngine* TestInterfaces::GetThemeEngine() {
  if (!test_runner_->UseMockTheme())
    return 0;
  if (!theme_engine_.get())
    theme_engine_.reset(new MockWebThemeEngine());
  return theme_engine_.get();
}

AppBannerClient* TestInterfaces::GetAppBannerClient() {
  return app_banner_client_;
}

}  // namespace test_runner
