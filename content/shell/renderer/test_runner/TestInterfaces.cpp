// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/TestInterfaces.h"

#include <string>

#include "base/logging.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/test_runner/accessibility_controller.h"
#include "content/shell/renderer/test_runner/event_sender.h"
#include "content/shell/renderer/test_runner/gamepad_controller.h"
#include "content/shell/renderer/test_runner/text_input_controller.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"

using namespace blink;

namespace content {

TestInterfaces::TestInterfaces()
    : m_accessibilityController(new AccessibilityController())
    , m_eventSender(new EventSender(this))
    , m_gamepadController(new GamepadController())
    , m_textInputController(new TextInputController())
    , m_testRunner(new TestRunner(this))
    , m_delegate(0)
{
    blink::setLayoutTestMode(true);
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableFontAntialiasing))
        blink::setFontAntialiasingEnabledForTest(true);

    // NOTE: please don't put feature specific enable flags here,
    // instead add them to RuntimeEnabledFeatures.in

    resetAll();
}

TestInterfaces::~TestInterfaces()
{
    m_accessibilityController->SetWebView(0);
    m_eventSender->SetWebView(0);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->SetWebView(NULL);
    m_testRunner->SetWebView(0, 0);

    m_accessibilityController->SetDelegate(0);
    m_eventSender->SetDelegate(0);
    m_gamepadController->SetDelegate(0);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->SetDelegate(0);
}

void TestInterfaces::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_proxy = proxy;
    m_accessibilityController->SetWebView(webView);
    m_eventSender->SetWebView(webView);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->SetWebView(webView);
    m_testRunner->SetWebView(webView, proxy);
}

void TestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_accessibilityController->SetDelegate(delegate);
    m_eventSender->SetDelegate(delegate);
    m_gamepadController->SetDelegate(delegate);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->SetDelegate(delegate);
    m_delegate = delegate;
}

void TestInterfaces::bindTo(WebFrame* frame)
{
    m_accessibilityController->Install(frame);
    m_eventSender->Install(frame);
    m_gamepadController->Install(frame);
    m_textInputController->Install(frame);
    m_testRunner->Install(frame);
}

void TestInterfaces::resetTestHelperControllers()
{
    m_accessibilityController->Reset();
    m_eventSender->Reset();
    m_gamepadController->Reset();
    // m_textInputController doesn't have any state to reset.
    WebCache::clear();
}

void TestInterfaces::resetAll()
{
    resetTestHelperControllers();
    m_testRunner->Reset();
}

void TestInterfaces::setTestIsRunning(bool running)
{
    m_testRunner->SetTestIsRunning(running);
}

void TestInterfaces::configureForTestWithURL(const WebURL& testURL, bool generatePixels)
{
    std::string spec = GURL(testURL).spec();
    m_testRunner->setShouldGeneratePixelResults(generatePixels);
    if (spec.find("loading/") != std::string::npos)
        m_testRunner->setShouldDumpFrameLoadCallbacks(true);
    if (spec.find("/dumpAsText/") != std::string::npos) {
        m_testRunner->setShouldDumpAsText(true);
        m_testRunner->setShouldGeneratePixelResults(false);
    }
    if (spec.find("/inspector/") != std::string::npos
        || spec.find("/inspector-enabled/") != std::string::npos)
        m_testRunner->clearDevToolsLocalStorage();
    if (spec.find("/inspector/") != std::string::npos) {
        // Subfolder name determines default panel to open.
        std::string settings = "";
        std::string test_path = spec.substr(spec.find("/inspector/") + 11);
        size_t slash_index = test_path.find("/");
        if (slash_index != std::string::npos) {
            settings = base::StringPrintf(
                "{\"lastActivePanel\":\"\\\"%s\\\"\"}",
                test_path.substr(0, slash_index).c_str());
        }
        m_testRunner->showDevTools(settings, std::string());
    }
    if (spec.find("/viewsource/") != std::string::npos) {
        m_testRunner->setShouldEnableViewSource(true);
        m_testRunner->setShouldGeneratePixelResults(false);
        m_testRunner->setShouldDumpAsMarkup(true);
    }
}

void TestInterfaces::windowOpened(WebTestProxyBase* proxy)
{
    m_windowList.push_back(proxy);
}

void TestInterfaces::windowClosed(WebTestProxyBase* proxy)
{
    std::vector<WebTestProxyBase*>::iterator pos = std::find(m_windowList.begin(), m_windowList.end(), proxy);
    if (pos == m_windowList.end()) {
        NOTREACHED();
        return;
    }
    m_windowList.erase(pos);
}

AccessibilityController* TestInterfaces::accessibilityController()
{
    return m_accessibilityController.get();
}

EventSender* TestInterfaces::eventSender()
{
    return m_eventSender.get();
}

TestRunner* TestInterfaces::testRunner()
{
    return m_testRunner.get();
}

WebTestDelegate* TestInterfaces::delegate()
{
    return m_delegate;
}

WebTestProxyBase* TestInterfaces::proxy()
{
    return m_proxy;
}

const std::vector<WebTestProxyBase*>& TestInterfaces::windowList()
{
    return m_windowList;
}

WebThemeEngine* TestInterfaces::themeEngine()
{
    if (!m_testRunner->UseMockTheme())
        return 0;
#if defined(__APPLE__)
    if (!m_themeEngine.get())
        m_themeEngine.reset(new WebTestThemeEngineMac());
#else
    if (!m_themeEngine.get())
        m_themeEngine.reset(new MockWebThemeEngine());
#endif
    return m_themeEngine.get();
}

}  // namespace content
