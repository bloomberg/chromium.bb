// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBAXOBJECTPROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBAXOBJECTPROXY_H_

#include <vector>

#include "content/shell/renderer/test_runner/CppBoundClass.h"
#include "third_party/WebKit/public/web/WebAXObject.h"

namespace WebTestRunner {

class WebAXObjectProxy : public CppBoundClass {
public:
    class Factory {
    public:
        virtual ~Factory() { }
        virtual WebAXObjectProxy* getOrCreate(const blink::WebAXObject&) = 0;
    };

    WebAXObjectProxy(const blink::WebAXObject&, Factory*);
    virtual ~WebAXObjectProxy();

    virtual WebAXObjectProxy* getChildAtIndex(unsigned);
    virtual bool isRoot() const;
    virtual bool isEqual(const blink::WebAXObject&);

    virtual void notificationReceived(const char *notificationName);

protected:
    const blink::WebAXObject& accessibilityObject() const { return m_accessibilityObject; }

    Factory* factory() const { return m_factory; }

private:
    // Bound properties.
    void roleGetterCallback(CppVariant*);
    void titleGetterCallback(CppVariant*);
    void descriptionGetterCallback(CppVariant*);
    void helpTextGetterCallback(CppVariant*);
    void stringValueGetterCallback(CppVariant*);
    void xGetterCallback(CppVariant*);
    void yGetterCallback(CppVariant*);
    void widthGetterCallback(CppVariant*);
    void heightGetterCallback(CppVariant*);
    void intValueGetterCallback(CppVariant*);
    void minValueGetterCallback(CppVariant*);
    void maxValueGetterCallback(CppVariant*);
    void valueDescriptionGetterCallback(CppVariant*);
    void childrenCountGetterCallback(CppVariant*);
    void insertionPointLineNumberGetterCallback(CppVariant*);
    void selectedTextRangeGetterCallback(CppVariant*);
    void isEnabledGetterCallback(CppVariant*);
    void isRequiredGetterCallback(CppVariant*);
    void isFocusedGetterCallback(CppVariant*);
    void isFocusableGetterCallback(CppVariant*);
    void isSelectedGetterCallback(CppVariant*);
    void isSelectableGetterCallback(CppVariant*);
    void isMultiSelectableGetterCallback(CppVariant*);
    void isSelectedOptionActiveGetterCallback(CppVariant*);
    void isExpandedGetterCallback(CppVariant*);
    void isCheckedGetterCallback(CppVariant*);
    void isVisibleGetterCallback(CppVariant*);
    void isOffScreenGetterCallback(CppVariant*);
    void isCollapsedGetterCallback(CppVariant*);
    void hasPopupGetterCallback(CppVariant*);
    void isValidGetterCallback(CppVariant*);
    void isReadOnlyGetterCallback(CppVariant*);
    void orientationGetterCallback(CppVariant*);
    void clickPointXGetterCallback(CppVariant*);
    void clickPointYGetterCallback(CppVariant*);
    void rowCountGetterCallback(CppVariant*);
    void columnCountGetterCallback(CppVariant*);
    void isClickableGetterCallback(CppVariant*);

    // Bound methods.
    void allAttributesCallback(const CppArgumentList&, CppVariant*);
    void attributesOfChildrenCallback(const CppArgumentList&, CppVariant*);
    void lineForIndexCallback(const CppArgumentList&, CppVariant*);
    void boundsForRangeCallback(const CppArgumentList&, CppVariant*);
    void childAtIndexCallback(const CppArgumentList&, CppVariant*);
    void elementAtPointCallback(const CppArgumentList&, CppVariant*);
    void tableHeaderCallback(const CppArgumentList&, CppVariant*);
    void rowIndexRangeCallback(const CppArgumentList&, CppVariant*);
    void columnIndexRangeCallback(const CppArgumentList&, CppVariant*);
    void cellForColumnAndRowCallback(const CppArgumentList&, CppVariant*);
    void titleUIElementCallback(const CppArgumentList&, CppVariant*);
    void setSelectedTextRangeCallback(const CppArgumentList&, CppVariant*);
    void isAttributeSettableCallback(const CppArgumentList&, CppVariant*);
    void isPressActionSupportedCallback(const CppArgumentList&, CppVariant*);
    void isIncrementActionSupportedCallback(const CppArgumentList&, CppVariant*);
    void isDecrementActionSupportedCallback(const CppArgumentList&, CppVariant*);
    void parentElementCallback(const CppArgumentList&, CppVariant*);
    void incrementCallback(const CppArgumentList&, CppVariant*);
    void decrementCallback(const CppArgumentList&, CppVariant*);
    void showMenuCallback(const CppArgumentList&, CppVariant*);
    void pressCallback(const CppArgumentList&, CppVariant*);
    void isEqualCallback(const CppArgumentList&, CppVariant*);
    void addNotificationListenerCallback(const CppArgumentList&, CppVariant*);
    void removeNotificationListenerCallback(const CppArgumentList&, CppVariant*);
    void takeFocusCallback(const CppArgumentList&, CppVariant*);
    void scrollToMakeVisibleCallback(const CppArgumentList&, CppVariant*);
    void scrollToMakeVisibleWithSubFocusCallback(const CppArgumentList&, CppVariant*);
    void scrollToGlobalPointCallback(const CppArgumentList&, CppVariant*);
    void wordStartCallback(const CppArgumentList&, CppVariant*);
    void wordEndCallback(const CppArgumentList&, CppVariant*);

    void fallbackCallback(const CppArgumentList&, CppVariant*);

    blink::WebAXObject m_accessibilityObject;
    Factory* m_factory;
    std::vector<CppVariant> m_notificationCallbacks;
};


class RootWebAXObjectProxy : public WebAXObjectProxy {
public:
    RootWebAXObjectProxy(const blink::WebAXObject&, Factory*);

    virtual WebAXObjectProxy* getChildAtIndex(unsigned) OVERRIDE;
    virtual bool isRoot() const OVERRIDE;
};


// Provides simple lifetime management of the WebAXObjectProxy instances:
// all WebAXObjectProxys ever created from the controller are stored in
// a list and cleared explicitly.
class WebAXObjectProxyList : public WebAXObjectProxy::Factory {
public:
    WebAXObjectProxyList();
    virtual ~WebAXObjectProxyList();

    void clear();
    virtual WebAXObjectProxy* getOrCreate(const blink::WebAXObject&) OVERRIDE;
    WebAXObjectProxy* createRoot(const blink::WebAXObject&);

private:
    typedef std::vector<WebAXObjectProxy*> ElementList;
    ElementList m_elements;
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBAXOBJECTPROXY_H_
