// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements the WebThemeEngine API used by the Windows version of
// Chromium to render native form controls like checkboxes, radio buttons,
// and scroll bars.
// The normal implementation (native_theme) renders the controls using either
// the UXTheme theming engine present in XP, Vista, and Win 7, or the "classic"
// theme used if that theme is selected in the Desktop settings.
// Unfortunately, both of these themes render controls differently on the
// different versions of Windows.
//
// In order to ensure maximum consistency of baselines across the different
// Windows versions, we provide a simple implementation for DRT here
// instead. These controls are actually platform-independent (they're rendered
// using Skia) and could be used on Linux and the Mac as well, should we
// choose to do so at some point.
//

#ifndef WebTestThemeEngineWin_h
#define WebTestThemeEngineWin_h

#include "third_party/WebKit/public/platform/WebNonCopyable.h"
#include "third_party/WebKit/public/platform/win/WebThemeEngine.h"

namespace WebTestRunner {

class WebTestThemeEngineWin : public blink::WebThemeEngine, public blink::WebNonCopyable {
public:
    WebTestThemeEngineWin() { }
    virtual ~WebTestThemeEngineWin() { }

    // WebThemeEngine methods:
    virtual void paintButton(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintMenuList(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarArrow(
        blink::WebCanvas*, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarThumb(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarTrack(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&, const blink::WebRect& alignRect);

    virtual void paintSpinButton(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintTextField(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&, blink::WebColor, bool fillContentArea,
        bool drawEdges);

    virtual void paintTrackbar(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintProgressBar(
        blink::WebCanvas*, const blink::WebRect& barRect,
        const blink::WebRect& valueRect,
        bool determinate, double time);

    virtual blink::WebSize getSize(int part);
};

}

#endif // WebTestThemeEngineWin_h
