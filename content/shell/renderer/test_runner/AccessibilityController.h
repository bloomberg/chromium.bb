// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITYCONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITYCONTROLLER_H_

#include "content/shell/renderer/test_runner/CppBoundClass.h"
#include "content/shell/renderer/test_runner/WebAXObjectProxy.h"

namespace blink {
class WebAXObject;
class WebFrame;
class WebView;
}

namespace WebTestRunner {

class WebTestDelegate;

class AccessibilityController : public CppBoundClass {
public:
    AccessibilityController();
    virtual ~AccessibilityController();

    // Shadow to include accessibility initialization.
    void bindToJavascript(blink::WebFrame*, const blink::WebString& classname);
    void reset();

    void setFocusedElement(const blink::WebAXObject&);
    WebAXObjectProxy* getFocusedElement();
    WebAXObjectProxy* getRootElement();
    WebAXObjectProxy* getAccessibleElementById(const std::string& id);

    bool shouldLogAccessibilityEvents();

    void notificationReceived(const blink::WebAXObject& target, const char* notificationName);

    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }
    void setWebView(blink::WebView* webView) { m_webView = webView; }

private:
    // If true, will log all accessibility notifications.
    bool m_logAccessibilityEvents;

    // Bound methods and properties
    void logAccessibilityEventsCallback(const CppArgumentList&, CppVariant*);
    void fallbackCallback(const CppArgumentList&, CppVariant*);
    void addNotificationListenerCallback(const CppArgumentList&, CppVariant*);
    void removeNotificationListenerCallback(const CppArgumentList&, CppVariant*);

    void focusedElementGetterCallback(CppVariant*);
    void rootElementGetterCallback(CppVariant*);
    void accessibleElementByIdGetterCallback(const CppArgumentList&, CppVariant*);

    WebAXObjectProxy* findAccessibleElementByIdRecursive(const blink::WebAXObject&, const blink::WebString& id);

    blink::WebAXObject m_focusedElement;
    blink::WebAXObject m_rootElement;

    WebAXObjectProxyList m_elements;

    std::vector<CppVariant> m_notificationCallbacks;

    WebTestDelegate* m_delegate;
    blink::WebView* m_webView;
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITYCONTROLLER_H_
