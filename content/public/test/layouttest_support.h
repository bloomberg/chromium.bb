// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include "base/callback_forward.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace blink {
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGamepad;
class WebGamepads;
struct WebSize;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

class RenderView;

// Turn the browser process into layout test mode.
void EnableBrowserLayoutTestMode();

///////////////////////////////////////////////////////////////////////////////
// The following methods are meant to be used from a renderer.

// Turn a renderer into layout test mode.
void EnableRendererLayoutTestMode();

// Enable injecting of a WebTestProxy between WebViews and RenderViews.
// |callback| is invoked with a pointer to WebTestProxyBase for each created
// WebTestProxy.
void EnableWebTestProxyCreation(const base::Callback<
    void(RenderView*, WebTestRunner::WebTestProxyBase*)>& callback);

// Sets the WebGamepads that should be returned by
// WebKitPlatformSupport::sampleGamepads().
void SetMockGamepads(const blink::WebGamepads& pads);

// Notifies blink about a new gamepad.
void MockGamepadConnected(int index, const blink::WebGamepad& pad);

// Notifies blink that a gamepad has been disconnected.
void MockGamepadDisconnected(int index, const blink::WebGamepad& pad);

// Sets WebDeviceMotionData that should be used when registering
// a listener through WebKitPlatformSupport::setDeviceMotionListener().
void SetMockDeviceMotionData(const blink::WebDeviceMotionData& data);

// Sets WebDeviceOrientationData that should be used when registering
// a listener through WebKitPlatformSupport::setDeviceOrientationListener().
void SetMockDeviceOrientationData(const blink::WebDeviceOrientationData& data);

// Sets WebScreenOrientation that should be used when registering a listener
// through WebKitPlatformSupport::setScreenOrientationListener().
void SetMockScreenOrientation(
    const blink::WebScreenOrientationType& orientation);

// Returns the length of the local session history of a render view.
int GetLocalSessionHistoryLength(RenderView* render_view);

// Sync the current session history to the browser process.
void SyncNavigationState(RenderView* render_view);

// Sets the focus of the render view depending on |enable|. This only overrides
// the state of the renderer, and does not sync the focus to the browser
// process.
void SetFocusAndActivate(RenderView* render_view, bool enable);

// Changes the window rect of the given render view.
void ForceResizeRenderView(RenderView* render_view,
                           const blink::WebSize& new_size);

// Set the device scale factor and force the compositor to resize.
void SetDeviceScaleFactor(RenderView* render_view, float factor);

// Enables or disables synchronous resize mode. When enabled, all window-sizing
// machinery is short-circuited inside the renderer. This mode is necessary for
// some tests that were written before browsers had multi-process architecture
// and rely on window resizes to happen synchronously.
// See http://crbug.com/309760 for details.
void UseSynchronousResizeMode(RenderView* render_view, bool enable);

// Control auto resize mode.
void EnableAutoResizeMode(RenderView* render_view,
                          const blink::WebSize& min_size,
                          const blink::WebSize& max_size);
void DisableAutoResizeMode(RenderView* render_view,
                           const blink::WebSize& new_size);

// Forces the |render_view| to use mock media streams.
void UseMockMediaStreams(RenderView* render_view);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
