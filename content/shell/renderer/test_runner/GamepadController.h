// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadController_h
#define GamepadController_h

#include "content/shell/renderer/test_runner/CppBoundClass.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace blink {
class WebGamepads;
class WebFrame;
}

namespace WebTestRunner {

class WebTestDelegate;

class GamepadController : public CppBoundClass {
public:
    GamepadController();

    void bindToJavascript(blink::WebFrame*, const blink::WebString& classname);
    void setDelegate(WebTestDelegate*);
    void reset();

private:
    // Bound methods and properties
    void connect(const CppArgumentList&, CppVariant*);
    void disconnect(const CppArgumentList&, CppVariant*);
    void setId(const CppArgumentList&, CppVariant*);
    void setButtonCount(const CppArgumentList&, CppVariant*);
    void setButtonData(const CppArgumentList&, CppVariant*);
    void setAxisCount(const CppArgumentList&, CppVariant*);
    void setAxisData(const CppArgumentList&, CppVariant*);
    void fallbackCallback(const CppArgumentList&, CppVariant*);

    blink::WebGamepads m_gamepads;

    WebTestDelegate* m_delegate;
};

}

#endif // GamepadController_h
