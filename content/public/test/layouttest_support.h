// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include "base/callback_forward.h"

namespace WebKit {
class WebGamepads;
struct WebSize;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

class RenderView;

// Enable injecting of a WebTestProxy between WebViews and RenderViews.
// |callback| is invoked with a pointer to WebTestProxyBase for each created
// WebTestProxy.
void EnableWebTestProxyCreation(const base::Callback<
    void(RenderView*, WebTestRunner::WebTestProxyBase*)>& callback);

// Sets the WebGamepads that should be returned by
// WebKitPlatformSupport::sampleGamepads().
void SetMockGamepads(const WebKit::WebGamepads& pads);

// Disable logging to the console from the appcache system.
void DisableAppCacheLogging();

// Enable testing support in the devtools client.
void EnableDevToolsFrontendTesting();

// Returns the length of the local session history of a render view.
int GetLocalSessionHistoryLength(RenderView* render_view);

void SetAllowOSMesaImageTransportForTesting();

// Suppress sending focus events from the renderer to the browser.
void DoNotSendFocusEvents();

// Sync the current session history to the browser process.
void SyncNavigationState(RenderView* render_view);

// Sets the focus of the render view depending on |enable|. This only overrides
// the state of the renderer, and does not sync the focus to the browser
// process.
void SetFocusAndActivate(RenderView* render_view, bool enable);

// When WebKit requests a size change, immediately report the new sizes back to
// WebKit instead of waiting for the browser to acknowledge the new size.
void EnableShortCircuitSizeUpdates();

// Changes the window rect of the given render view.
void ForceResizeRenderView(RenderView* render_view,
                           const WebKit::WebSize& new_size);

// Never display error pages when a navigation fails.
void DisableNavigationErrorPages();

// Set the device scale factor and force the compositor to resize.
void SetDeviceScaleFactor(RenderView* render_view, float factor);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
