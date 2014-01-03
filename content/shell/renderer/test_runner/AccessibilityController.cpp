// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/AccessibilityController.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"

using namespace blink;

namespace WebTestRunner {

AccessibilityController::AccessibilityController()
    : m_logAccessibilityEvents(false)
{

    bindMethod("logAccessibilityEvents", &AccessibilityController::logAccessibilityEventsCallback);
    bindMethod("addNotificationListener", &AccessibilityController::addNotificationListenerCallback);
    bindMethod("removeNotificationListener", &AccessibilityController::removeNotificationListenerCallback);

    bindProperty("focusedElement", &AccessibilityController::focusedElementGetterCallback);
    bindProperty("rootElement", &AccessibilityController::rootElementGetterCallback);

    bindMethod("accessibleElementById", &AccessibilityController::accessibleElementByIdGetterCallback);

    bindFallbackMethod(&AccessibilityController::fallbackCallback);
}

AccessibilityController::~AccessibilityController()
{
}

void AccessibilityController::bindToJavascript(WebFrame* frame, const WebString& classname)
{
    WebAXObject::enableAccessibility();
    WebAXObject::enableInlineTextBoxAccessibility();
    CppBoundClass::bindToJavascript(frame, classname);
}

void AccessibilityController::reset()
{
    m_rootElement = WebAXObject();
    m_focusedElement = WebAXObject();
    m_elements.clear();
    m_notificationCallbacks.clear();

    m_logAccessibilityEvents = false;
}

void AccessibilityController::setFocusedElement(const WebAXObject& focusedElement)
{
    m_focusedElement = focusedElement;
}

WebAXObjectProxy* AccessibilityController::getFocusedElement()
{
    if (m_focusedElement.isNull())
        m_focusedElement = m_webView->accessibilityObject();
    return m_elements.getOrCreate(m_focusedElement);
}

WebAXObjectProxy* AccessibilityController::getRootElement()
{
    if (m_rootElement.isNull())
        m_rootElement = m_webView->accessibilityObject();
    return m_elements.createRoot(m_rootElement);
}

WebAXObjectProxy* AccessibilityController::findAccessibleElementByIdRecursive(const WebAXObject& obj, const WebString& id)
{
    if (obj.isNull() || obj.isDetached())
        return 0;

    WebNode node = obj.node();
    if (!node.isNull() && node.isElementNode()) {
        WebElement element = node.to<WebElement>();
        element.getAttribute("id");
        if (element.getAttribute("id") == id)
            return m_elements.getOrCreate(obj);
    }

    unsigned childCount = obj.childCount();
    for (unsigned i = 0; i < childCount; i++) {
        if (WebAXObjectProxy* result = findAccessibleElementByIdRecursive(obj.childAt(i), id))
            return result;
    }

    return 0;
}

WebAXObjectProxy* AccessibilityController::getAccessibleElementById(const std::string& id)
{
    if (m_rootElement.isNull())
        m_rootElement = m_webView->accessibilityObject();

    if (!m_rootElement.updateBackingStoreAndCheckValidity())
        return 0;

    return findAccessibleElementByIdRecursive(m_rootElement, WebString::fromUTF8(id.c_str()));
}

bool AccessibilityController::shouldLogAccessibilityEvents()
{
    return m_logAccessibilityEvents;
}

void AccessibilityController::notificationReceived(const blink::WebAXObject& target, const char* notificationName)
{
    // Call notification listeners on the element.
    WebAXObjectProxy* element = m_elements.getOrCreate(target);
    element->notificationReceived(notificationName);

    // Call global notification listeners.
    size_t callbackCount = m_notificationCallbacks.size();
    for (size_t i = 0; i < callbackCount; i++) {
        CppVariant arguments[2];
        arguments[0].set(*element->getAsCppVariant());
        arguments[1].set(notificationName);
        CppVariant invokeResult;
        m_notificationCallbacks[i].invokeDefault(arguments, 2, invokeResult);
    }
}

void AccessibilityController::logAccessibilityEventsCallback(const CppArgumentList&, CppVariant* result)
{
    m_logAccessibilityEvents = true;
    result->setNull();
}

void AccessibilityController::addNotificationListenerCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    m_notificationCallbacks.push_back(arguments[0]);
    result->setNull();
}

void AccessibilityController::removeNotificationListenerCallback(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void AccessibilityController::focusedElementGetterCallback(CppVariant* result)
{
    result->set(*(getFocusedElement()->getAsCppVariant()));
}

void AccessibilityController::rootElementGetterCallback(CppVariant* result)
{
    result->set(*(getRootElement()->getAsCppVariant()));
}

void AccessibilityController::accessibleElementByIdGetterCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    std::string id = arguments[0].toString();
    WebAXObjectProxy* foundElement = getAccessibleElementById(id);
    if (!foundElement)
        return;

    result->set(*(foundElement->getAsCppVariant()));
}

void AccessibilityController::fallbackCallback(const CppArgumentList&, CppVariant* result)
{
    m_delegate->printMessage("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on AccessibilityController\n");
    result->setNull();
}

}
