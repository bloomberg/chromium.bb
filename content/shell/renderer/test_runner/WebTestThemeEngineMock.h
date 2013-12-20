// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTestThemeEngineMock_h
#define WebTestThemeEngineMock_h

#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/default/WebThemeEngine.h"

using blink::WebRect;
using blink::WebThemeEngine;

namespace WebTestRunner {

class WebTestThemeEngineMock : public blink::WebThemeEngine {
public:
    virtual ~WebTestThemeEngineMock() { }

    // WebThemeEngine methods:
    virtual blink::WebSize getSize(WebThemeEngine::Part);

    virtual void paint(blink::WebCanvas*,
        WebThemeEngine::Part,
        WebThemeEngine::State,
        const blink::WebRect&,
        const WebThemeEngine::ExtraParams*);
};

} // namespace WebTestRunner

#endif // WebTestThemeEngineMock_h
