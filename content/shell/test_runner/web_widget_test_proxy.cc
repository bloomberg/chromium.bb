// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_widget_test_proxy.h"

#include "content/renderer/compositor/layer_tree_view.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace test_runner {

void WebWidgetTestProxy::ScheduleAnimation() {
  if (!GetTestRunner()->TestIsRunning())
    return;

  // When using threaded compositing, have the RenderWidget schedule a request
  // for a frame, as we use the compositor's scheduler. Otherwise the testing
  // WebWidgetClient schedules it.
  // Note that for WebWidgetTestProxy the RenderWidget is subclassed to override
  // the WebWidgetClient, so we must call up to the base class RenderWidget
  // explicitly here to jump out of the test harness as intended.
  if (!RenderWidget::layer_tree_view()->CompositeIsSynchronousForTesting()) {
    RenderWidget::ScheduleAnimation();
    return;
  }

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    GetWebViewTestProxy()->delegate()->PostDelayedTask(
        base::BindOnce(&WebWidgetTestProxy::AnimateNow,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(1));
  }
}

bool WebWidgetTestProxy::RequestPointerLock() {
  return GetViewTestRunner()->RequestPointerLock();
}

void WebWidgetTestProxy::RequestPointerUnlock() {
  return GetViewTestRunner()->RequestPointerUnlock();
}

bool WebWidgetTestProxy::IsPointerLocked() {
  return GetViewTestRunner()->isPointerLocked();
}

void WebWidgetTestProxy::SetToolTipText(const blink::WebString& text,
                                        blink::WebTextDirection hint) {
  RenderWidget::SetToolTipText(text, hint);
  GetTestRunner()->setToolTipText(text);
}

void WebWidgetTestProxy::StartDragging(network::mojom::ReferrerPolicy policy,
                                       const blink::WebDragData& data,
                                       blink::WebDragOperationsMask mask,
                                       const SkBitmap& drag_image,
                                       const gfx::Point& image_offset) {
  GetTestRunner()->setDragImage(drag_image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  event_sender()->DoDragDrop(data, mask);
}

WebViewTestProxy* WebWidgetTestProxy::GetWebViewTestProxy() {
  if (delegate()) {
    // TODO(https://crbug.com/545684): Because WebViewImpl still inherits
    // from WebWidget and infact, before a frame is attached, IS actually
    // the WebWidget used in blink, it is not possible to walk the object
    // relations in the blink side to find the associated RenderViewImpl
    // consistently. Thus, here, just directly cast the delegate(). Since
    // all creations of RenderViewImpl in the test_runner layer haved been
    // shimmed to return a WebViewTestProxy, it is safe to downcast here.
    return static_cast<WebViewTestProxy*>(delegate());
  } else {
    blink::WebWidget* web_widget = GetWebWidget();
    CHECK(web_widget->IsWebFrameWidget());
    blink::WebView* web_view =
        static_cast<blink::WebFrameWidget*>(web_widget)->LocalRoot()->View();

    content::RenderView* render_view =
        content::RenderView::FromWebView(web_view);
    // RenderViews are always WebViewTestProxy within the test_runner namespace.
    return static_cast<WebViewTestProxy*>(render_view);
  }
}

void WebWidgetTestProxy::Reset() {
  event_sender_.Reset();
}

void WebWidgetTestProxy::BindTo(blink::WebLocalFrame* frame) {
  event_sender_.Install(frame);
}

WebWidgetTestProxy::~WebWidgetTestProxy() = default;

TestRunnerForSpecificView* WebWidgetTestProxy::GetViewTestRunner() {
  return GetWebViewTestProxy()->view_test_runner();
}

TestRunner* WebWidgetTestProxy::GetTestRunner() {
  return GetWebViewTestProxy()->test_interfaces()->GetTestRunner();
}

void WebWidgetTestProxy::AnimateNow() {
  if (!animation_scheduled_)
    return;

  // For child local roots, it's possible that the backing WebWidget gets
  // closed between the ScheduleAnimation() call and this execution
  // leading to a nullptr.  This happens because child local roots are
  // owned by RenderFrames which drops the WebWidget before executing the
  // Close() call that would invalidate the |weak_factory_| canceling the
  // scheduled calls to AnimateNow(). In main frames, the WebWidget is
  // dropped synchronously by Close() avoiding the problem.
  //
  // Ideally there would not be this divergence between frame types, and/or
  // there would be a hook for signaling a stop to the animation. As this is
  // not an ideal work, returning early when GetWebWidget() returns nullptr
  // is good enough for a fake impl. Also, unicorns are mythical. Sorry.
  if (!GetWebWidget())
    return;

  animation_scheduled_ = false;
  CHECK(GetTestRunner());
  bool animation_requires_raster = GetTestRunner()->animation_requires_raster();
  blink::WebWidget* web_widget = GetWebWidget();
  CHECK(web_widget);
  web_widget->UpdateAllLifecyclePhasesAndCompositeForTesting(
      animation_requires_raster);

  // If this RenderWidget is attached to a RenderView, we composite the
  // current PagePopup with the widget.
  //
  // TODO(danakj): This means that an OOPIF's popup, which is attached to a
  // WebView without a main frame, would have no opportunity to execute this
  // method call.
  if (delegate()) {
    blink::WebView* view = GetWebViewTestProxy()->webview();
    CHECK(view);
    if (blink::WebPagePopup* popup = view->GetPagePopup()) {
      popup->UpdateAllLifecyclePhasesAndCompositeForTesting(
          animation_requires_raster);
    }
  }
}

}  // namespace test_runner
