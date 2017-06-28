// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_render_frame_observer.h"

#include <string>

#include "content/public/common/associated_interface_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/renderer/layout_test/blink_test_runner.h"
#include "content/shell/renderer/layout_test/layout_test_render_thread_observer.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

LayoutTestRenderFrameObserver::LayoutTestRenderFrameObserver(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), binding_(this) {
  test_runner::WebTestRunner* test_runner =
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner();
  render_frame->GetWebFrame()->SetContentSettingsClient(
      test_runner->GetWebContentSettings());
  render_frame->GetWebFrame()->SetTextCheckClient(
      test_runner->GetWebTextCheckClient());
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &LayoutTestRenderFrameObserver::BindRequest, base::Unretained(this)));
}

LayoutTestRenderFrameObserver::~LayoutTestRenderFrameObserver() = default;

void LayoutTestRenderFrameObserver::BindRequest(
    mojom::LayoutTestControlAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void LayoutTestRenderFrameObserver::OnDestruct() {
  delete this;
}

void LayoutTestRenderFrameObserver::DumpFrameLayout(
    DumpFrameLayoutCallback callback) {
  std::string dump =
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner()
          ->DumpLayout(render_frame()->GetWebFrame());
  std::move(callback).Run(dump);
}

void LayoutTestRenderFrameObserver::ReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnReplicateTestConfiguration(std::move(config));
}

void LayoutTestRenderFrameObserver::SetTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetTestConfiguration(std::move(config));
}

void LayoutTestRenderFrameObserver::SetupSecondaryRenderer() {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetupSecondaryRenderer();
}

}  // namespace content
