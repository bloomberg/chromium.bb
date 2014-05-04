// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements the WebThemeEngine API in such a way that we match the Mac
// port rendering more than usual Chromium path, thus allowing us to share
// more pixel baselines.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMAC_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMAC_H_

#include "base/basictypes.h"
#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace content {

class WebTestThemeEngineMac : public blink::WebThemeEngine {
public:
    WebTestThemeEngineMac() { }
    virtual ~WebTestThemeEngineMac() { }

    virtual void paintScrollbarThumb(
        blink::WebCanvas*,
        blink::WebThemeEngine::State,
        blink::WebThemeEngine::Size,
        const blink::WebRect&,
        const blink::WebThemeEngine::ScrollbarInfo&);

private:
    virtual void paintHIThemeScrollbarThumb(
        blink::WebCanvas*,
        blink::WebThemeEngine::State,
        blink::WebThemeEngine::Size,
        const blink::WebRect&,
        const blink::WebThemeEngine::ScrollbarInfo&);
    virtual void paintNSScrollerScrollbarThumb(
        blink::WebCanvas*,
        blink::WebThemeEngine::State,
        blink::WebThemeEngine::Size,
        const blink::WebRect&,
        const blink::WebThemeEngine::ScrollbarInfo&);

    DISALLOW_COPY_AND_ASSIGN(WebTestThemeEngineMac);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTTHEMEENGINEMAC_H_
