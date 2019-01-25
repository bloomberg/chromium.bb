// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_render_frame_observer.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebTestRenderFrameObserver::WebTestRenderFrameObserver(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), binding_(this) {
  test_runner::WebTestRunner* test_runner =
      WebTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner();
  render_frame->GetWebFrame()->SetContentSettingsClient(
      test_runner->GetWebContentSettings());
  render_frame->GetWebFrame()->SetTextCheckClient(
      test_runner->GetWebTextCheckClient());
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &WebTestRenderFrameObserver::BindRequest, base::Unretained(this)));
}

WebTestRenderFrameObserver::~WebTestRenderFrameObserver() = default;

void WebTestRenderFrameObserver::BindRequest(
    mojom::WebTestControlAssociatedRequest request) {
  binding_.Bind(std::move(request),
                blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebTestRenderFrameObserver::OnDestruct() {
  delete this;
}

void WebTestRenderFrameObserver::CaptureDump(CaptureDumpCallback callback) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->CaptureDump(std::move(callback));
}

void WebTestRenderFrameObserver::CompositeWithRaster(
    CompositeWithRasterCallback callback) {
  blink::WebWidget* widget = render_frame()->GetWebFrame()->FrameWidget();
  blink::WebView* view = render_frame()->GetWebFrame()->View();
  if (widget) {
    widget->UpdateAllLifecyclePhasesAndCompositeForTesting(/*do_raster=*/true);

    // The current PagePopup is composited together with the main frame.
    // TODO(danakj): This means that an OOPIF's popup, which is attached to a
    // WebView without a main frame, would have no opportunity to execute this
    // method call.
    if (render_frame()->IsMainFrame()) {
      if (blink::WebPagePopup* popup = view->GetPagePopup()) {
        popup->UpdateAllLifecyclePhasesAndCompositeForTesting(
            /*do_raster=*/true);
      }
    }
  }
  std::move(callback).Run();
}

void WebTestRenderFrameObserver::DumpFrameLayout(
    DumpFrameLayoutCallback callback) {
  std::string dump = WebTestRenderThreadObserver::GetInstance()
                         ->test_interfaces()
                         ->TestRunner()
                         ->DumpLayout(render_frame()->GetWebFrame());
  std::move(callback).Run(dump);
}

void WebTestRenderFrameObserver::ReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnReplicateTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetupSecondaryRenderer() {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetupSecondaryRenderer();
}

}  // namespace content
