// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMOCK_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMOCK_H_

#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebThemeEngine.h"

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

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMOCK_H_
