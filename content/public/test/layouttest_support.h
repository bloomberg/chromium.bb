// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace WebKit {
class WebGamepads;
struct WebSize;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace base {
class MessageLoopProxy;
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
void SetMockGamepads(const WebKit::WebGamepads& pads);

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
                           const WebKit::WebSize& new_size);

// Set the device scale factor and force the compositor to resize.
void SetDeviceScaleFactor(RenderView* render_view, float factor);

// Control auto resize mode.
void EnableAutoResizeMode(RenderView* render_view,
                          const WebKit::WebSize& min_size,
                          const WebKit::WebSize& max_size);
void DisableAutoResizeMode(RenderView* render_view,
                           const WebKit::WebSize& new_size);

// Return the thread on which media operations should run.
//
// TODO(scherkus): We should be using RenderViewImpl::createMediaPlayer(), see
// http://crbug.com/239826
scoped_refptr<base::MessageLoopProxy> GetMediaThreadMessageLoopProxy();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
