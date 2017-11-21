// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "cc/layers/texture_layer.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

class GURL;

namespace blink {
class WebInputEvent;
class WebLocalFrame;
struct WebSize;
class WebURLRequest;
class WebView;
class WebWidget;
class WebURLResponse;
}

namespace device {
class MotionData;
class OrientationData;
}

namespace gfx {
class ColorSpace;
}

namespace test_runner {
class WebFrameTestProxyBase;
class WebViewTestProxyBase;
class WebWidgetTestProxyBase;
}

namespace content {

class RenderFrame;
class RendererGamepadProvider;
class RenderView;

// Turn the browser process into layout test mode.
void EnableBrowserLayoutTestMode();

// Terminates all workers and notifies when complete. This is used for
// testing when it is important to make sure that all shared worker activity
// has stopped.
void TerminateAllSharedWorkersForTesting(base::OnceClosure callback);

///////////////////////////////////////////////////////////////////////////////
// The following methods are meant to be used from a renderer.

// Turn a renderer into layout test mode.
void EnableRendererLayoutTestMode();

// "Casts" |render_view| to |WebViewTestProxyBase|.  Caller has to ensure that
// prior to construction of |render_view|, EnableWebTestProxyCreation was
// called.
test_runner::WebViewTestProxyBase* GetWebViewTestProxyBase(
    RenderView* render_view);

// "Casts" |render_frame| to |WebFrameTestProxyBase|.  Caller has to ensure
// that prior to construction of |render_frame|, EnableTestProxyCreation
// was called.
test_runner::WebFrameTestProxyBase* GetWebFrameTestProxyBase(
    RenderFrame* render_frame);

// Gets WebWidgetTestProxyBase associated with |frame| (either the view's widget
// or the local root's frame widget).  Caller has to ensure that prior to
// construction of |render_frame|, EnableTestProxyCreation was called.
test_runner::WebWidgetTestProxyBase* GetWebWidgetTestProxyBase(
    blink::WebLocalFrame* frame);

// Enable injecting of a WebViewTestProxy between WebViews and RenderViews,
// WebWidgetTestProxy between WebWidgets and RenderWidgets and WebFrameTestProxy
// between WebFrames and RenderFrames.
// |view_proxy_creation_callback| is invoked after creating WebViewTestProxy.
// |widget_proxy_creation_callback| is invoked after creating
// WebWidgetTestProxy.
// |frame_proxy_creation_callback| is called after creating WebFrameTestProxy.
using ViewProxyCreationCallback =
    base::Callback<void(RenderView*, test_runner::WebViewTestProxyBase*)>;
using WidgetProxyCreationCallback =
    base::Callback<void(blink::WebWidget*,
                        test_runner::WebWidgetTestProxyBase*)>;
using FrameProxyCreationCallback =
    base::Callback<void(RenderFrame*, test_runner::WebFrameTestProxyBase*)>;
void EnableWebTestProxyCreation(
    const ViewProxyCreationCallback& view_proxy_creation_callback,
    const WidgetProxyCreationCallback& widget_proxy_creation_callback,
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

// Sets MotionData that should be used when registering
// a listener through BlinkPlatformImpl::setDeviceMotionListener().
void SetMockDeviceMotionData(const device::MotionData& data);

// Sets OrientationData that should be used when registering
// a listener through BlinkPlatformImpl::setDeviceOrientationListener().
void SetMockDeviceOrientationData(const device::OrientationData& data);

// Returns the length of the local session history of a render view.
int GetLocalSessionHistoryLength(RenderView* render_view);

// Sets the focus of the render view depending on |enable|. This only overrides
// the state of the renderer, and does not sync the focus to the browser
// process.
void SetFocusAndActivate(RenderView* render_view, bool enable);

// Changes the window rect of the given render view.
void ForceResizeRenderView(RenderView* render_view,
                           const blink::WebSize& new_size);

// Set the device scale factor and force the compositor to resize.
void SetDeviceScaleFactor(RenderView* render_view, float factor);

// Get the window to viewport scale.
float GetWindowToViewportScale(RenderView* render_view);

// Converts |event| from screen coordinates to coordinates used by the widget
// associated with the |web_widget_test_proxy_base|.  Returns nullptr if no
// transformation was necessary (e.g. for a keyboard event OR if widget requires
// no scaling and has coordinates starting at (0,0)).
std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxyBase* web_widget_test_proxy_base,
    const blink::WebInputEvent& event);

// Get the color space for a given name string. This is not in the ColorSpace
// class to avoid bloating the shipping build.
gfx::ColorSpace GetTestingColorSpace(const std::string& name);

// Set the device color space.
void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space);

// Sets the scan duration to 0.
void SetTestBluetoothScanDuration();

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

// Run all pending idle tasks immediately, and then invoke callback.
void SchedulerRunIdleTasks(const base::Closure& callback);

// Causes the RenderWidget corresponding to |render_frame| to update its
// TextInputState.
void ForceTextInputStateUpdateForRenderFrame(RenderFrame* render_frame);

// PlzNavigate
// Returns true if the navigation identified by the |request| was initiated by
// the browser or renderer.
bool IsNavigationInitiatedByRenderer(const blink::WebURLRequest& request);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
