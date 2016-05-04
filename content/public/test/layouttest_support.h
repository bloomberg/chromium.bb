// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "cc/layers/texture_layer.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

class GURL;

namespace blink {
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGamepad;
class WebGamepads;
class WebLayer;
struct WebSize;
class WebView;
class WebURLResponse;
}

namespace device {
class BluetoothAdapter;
}

namespace test_runner {
class WebFrameTestProxyBase;
class WebTestProxyBase;
}

namespace content {

class PageState;
class RenderFrame;
class RendererGamepadProvider;
class RenderView;

// Turn the browser process into layout test mode.
void EnableBrowserLayoutTestMode();

///////////////////////////////////////////////////////////////////////////////
// The following methods are meant to be used from a renderer.

// Turn a renderer into layout test mode.
void EnableRendererLayoutTestMode();

// "Casts" |render_view| to |WebTestProxyBase|.  Caller has to ensure that prior
// to construction of |render_view|, EnableWebTestProxyCreation was called.
test_runner::WebTestProxyBase* GetWebTestProxyBase(RenderView* render_view);

// "Casts" |render_frame| to |WebFrameTestProxyBase|.  Caller has to ensure
// that prior to construction of |render_frame|, EnableWebTestProxyCreation was
// called.
test_runner::WebFrameTestProxyBase* GetWebFrameTestProxyBase(
    RenderFrame* render_frame);

// Enable injecting of a WebTestProxy between WebViews and RenderViews
// and WebFrameTestProxy between WebFrames and RenderFrames.
// |view_proxy_creation_callback| is invoked after creating WebTestProxy.
// |frame_proxy_creation_callback| is called after creating WebFrameTestProxy.
using ViewProxyCreationCallback =
    base::Callback<void(RenderView*, test_runner::WebTestProxyBase*)>;
using FrameProxyCreationCallback =
    base::Callback<void(RenderFrame*, test_runner::WebFrameTestProxyBase*)>;
void EnableWebTestProxyCreation(
    const ViewProxyCreationCallback& view_proxy_creation_callback,
    const FrameProxyCreationCallback& frame_proxy_creation_callback);

typedef base::Callback<void(const blink::WebURLResponse& response,
                            const std::string& data)> FetchManifestCallback;
void FetchManifest(blink::WebView* view, const GURL& url,
                   const FetchManifestCallback&);

// Sets gamepad provider to be used for layout tests.
void SetMockGamepadProvider(std::unique_ptr<RendererGamepadProvider> provider);

// Sets a double that should be used when registering
// a listener through BlinkPlatformImpl::setDeviceLightListener().
void SetMockDeviceLightData(const double data);

// Sets WebDeviceMotionData that should be used when registering
// a listener through BlinkPlatformImpl::setDeviceMotionListener().
void SetMockDeviceMotionData(const blink::WebDeviceMotionData& data);

// Sets WebDeviceOrientationData that should be used when registering
// a listener through BlinkPlatformImpl::setDeviceOrientationListener().
void SetMockDeviceOrientationData(const blink::WebDeviceOrientationData& data);

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

// Set the device color profile associated with the profile |name|.
void SetDeviceColorProfile(RenderView* render_view, const std::string& name);

// Change the bluetooth test adapter while running a layout test.
void SetBluetoothAdapter(int render_process_id,
                         scoped_refptr<device::BluetoothAdapter> adapter);

// Enables mock geofencing service while running a layout test.
// |service_available| indicates if the mock service should mock geofencing
// being available or not.
void SetGeofencingMockProvider(bool service_available);

// Disables mock geofencing service while running a layout test.
void ClearGeofencingMockProvider();

// Set the mock geofencing position while running a layout test.
void SetGeofencingMockPosition(double latitude, double longitude);

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

// Provides a text dump of the contents of the given page state.
std::string DumpBackForwardList(std::vector<PageState>& page_state,
                                size_t current_index);

// Run all pending idle tasks immediately, and then invoke callback.
void SchedulerRunIdleTasks(const base::Closure& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
