// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements the WebThemeEngine API in such a way that we match the Mac
// port rendering more than usual Chromium path, thus allowing us to share
// more pixel baselines.

#ifndef WebTestThemeEngineMac_h
#define WebTestThemeEngineMac_h

#include "third_party/WebKit/public/platform/WebNonCopyable.h"
#include "third_party/WebKit/public/platform/mac/WebThemeEngine.h"

namespace WebTestRunner {

class WebTestThemeEngineMac : public blink::WebThemeEngine, public blink::WebNonCopyable {
public:
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
};

}

#endif // WebTestThemeEngineMac_h
