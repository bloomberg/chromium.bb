// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TextInputController is bound to window.textInputController in Javascript
// when DRT is running. Layout tests use it to exercise various corners of
// text input.

#ifndef TextInputController_h
#define TextInputController_h

#include "content/shell/renderer/test_runner/CppBoundClass.h"

namespace blink {
class WebView;
}

namespace WebTestRunner {

class TextInputController : public CppBoundClass {
public:
    TextInputController();

    void setWebView(blink::WebView* webView) { m_webView = webView; }

    void insertText(const CppArgumentList&, CppVariant*);
    void doCommand(const CppArgumentList&, CppVariant*);
    void setMarkedText(const CppArgumentList&, CppVariant*);
    void unmarkText(const CppArgumentList&, CppVariant*);
    void hasMarkedText(const CppArgumentList&, CppVariant*);
    void markedRange(const CppArgumentList&, CppVariant*);
    void selectedRange(const CppArgumentList&, CppVariant*);
    void firstRectForCharacterRange(const CppArgumentList&, CppVariant*);
    void setComposition(const CppArgumentList&, CppVariant*);

private:
    blink::WebView* m_webView;
};

}

#endif // TextInputController_h
