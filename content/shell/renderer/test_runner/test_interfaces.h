// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_INTERFACES_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_INTERFACES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

#if defined(OS_MACOSX)
#include "content/shell/renderer/test_runner/mock_web_theme_engine_mac.h"
#else
#include "content/shell/renderer/test_runner/mock_web_theme_engine.h"
#endif

namespace blink {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace content {

class AccessibilityController;
class EventSender;
class GamepadController;
class TestRunner;
class TextInputController;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void SetWebView(blink::WebView* web_view, WebTestProxyBase* proxy);
  void SetDelegate(WebTestDelegate* delegate);
  void BindTo(blink::WebFrame* frame);
  void ResetTestHelperControllers();
  void ResetAll();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool generate_pixels);

  void WindowOpened(WebTestProxyBase* proxy);
  void WindowClosed(WebTestProxyBase* proxy);

  AccessibilityController* GetAccessibilityController();
  EventSender* GetEventSender();
  TestRunner* GetTestRunner();
  WebTestDelegate* GetDelegate();
  WebTestProxyBase* GetProxy();
  const std::vector<WebTestProxyBase*>& GetWindowList();
  blink::WebThemeEngine* GetThemeEngine();

 private:
  scoped_ptr<AccessibilityController> accessibility_controller_;
  scoped_ptr<EventSender> event_sender_;
  base::WeakPtr<GamepadController> gamepad_controller_;
  scoped_ptr<TextInputController> text_input_controller_;
  scoped_ptr<TestRunner> test_runner_;
  WebTestDelegate* delegate_;
  WebTestProxyBase* proxy_;

  std::vector<WebTestProxyBase*> window_list_;
#if defined(OS_MACOSX)
  scoped_ptr<MockWebThemeEngineMac> theme_engine_;
#else
  scoped_ptr<MockWebThemeEngine> theme_engine_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_INTERFACES_H_
