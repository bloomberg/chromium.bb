// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestInterfaces_h
#define TestInterfaces_h

#include <vector>

#include "content/shell/renderer/test_runner/WebScopedPtr.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

#if defined(USE_DEFAULT_RENDER_THEME)
#include "content/shell/renderer/test_runner/WebTestThemeEngineMock.h"
#elif defined(WIN32)
#include "content/shell/renderer/test_runner/WebTestThemeEngineWin.h"
#elif defined(__APPLE__)
#include "content/shell/renderer/test_runner/WebTestThemeEngineMac.h"
#endif

namespace blink {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace WebTestRunner {

class AccessibilityController;
class EventSender;
class GamepadController;
class TestRunner;
class TextInputController;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces : public blink::WebNonCopyable {
public:
    TestInterfaces();
    ~TestInterfaces();

    void setWebView(blink::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(blink::WebFrame*);
    void resetTestHelperControllers();
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const blink::WebURL&, bool generatePixels);

    void windowOpened(WebTestProxyBase*);
    void windowClosed(WebTestProxyBase*);

    AccessibilityController* accessibilityController();
    EventSender* eventSender();
    TestRunner* testRunner();
    WebTestDelegate* delegate();
    WebTestProxyBase* proxy();
    const std::vector<WebTestProxyBase*>& windowList();
    blink::WebThemeEngine* themeEngine();

private:
    WebScopedPtr<AccessibilityController> m_accessibilityController;
    WebScopedPtr<EventSender> m_eventSender;
    WebScopedPtr<GamepadController> m_gamepadController;
    WebScopedPtr<TextInputController> m_textInputController;
    WebScopedPtr<TestRunner> m_testRunner;
    WebTestDelegate* m_delegate;
    WebTestProxyBase* m_proxy;

    std::vector<WebTestProxyBase*> m_windowList;
#if defined(USE_DEFAULT_RENDER_THEME)
    WebScopedPtr<WebTestThemeEngineMock> m_themeEngine;
#elif defined(WIN32)
    WebScopedPtr<WebTestThemeEngineWin> m_themeEngine;
#elif defined(__APPLE__)
    WebScopedPtr<WebTestThemeEngineMac> m_themeEngine;
#endif
};

}

#endif // TestInterfaces_h
