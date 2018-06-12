// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/hit_test_region_observer.h"

#include <algorithm>

#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

void WaitForHitTestDataOrChildSurfaceReady(
    content::RenderFrameHost* child_frame) {
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderFrameHostImpl*>(child_frame)
          ->GetRenderWidgetHost()
          ->GetView();

  if (features::IsVizHitTestingEnabled()) {
    HitTestRegionObserver observer(child_view->GetFrameSinkId());
    observer.WaitForHitTestData();
    return;
  }

  WaitForChildFrameSurfaceReady(child_frame);
}

void WaitForHitTestDataOrGuestSurfaceReady(
    content::WebContents* guest_web_contents) {
  DCHECK(static_cast<RenderWidgetHostViewBase*>(
             guest_web_contents->GetRenderWidgetHostView())
             ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          guest_web_contents->GetRenderWidgetHostView());

  if (features::IsVizHitTestingEnabled()) {
    HitTestRegionObserver observer(child_view->GetFrameSinkId());
    observer.WaitForHitTestData();
    return;
  }

  WaitForGuestSurfaceReady(guest_web_contents);
}

HitTestRegionObserver::HitTestRegionObserver(
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id) {
  CHECK(frame_sink_id.is_valid());
  GetHostFrameSinkManager()->AddHitTestRegionObserver(this);
}

HitTestRegionObserver::~HitTestRegionObserver() {
  GetHostFrameSinkManager()->RemoveHitTestRegionObserver(this);
}

void HitTestRegionObserver::WaitForHitTestData() {
  for (auto& it : GetHostFrameSinkManager()->display_hit_test_query()) {
    if (it.second->ContainsFrameSinkId(frame_sink_id_)) {
      return;
    }
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void HitTestRegionObserver::OnAggregatedHitTestRegionListUpdated(
    const viz::FrameSinkId& frame_sink_id,
    const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) {
  if (!run_loop_)
    return;

  for (auto& it : hit_test_data) {
    if (it.frame_sink_id == frame_sink_id_) {
      run_loop_->Quit();
      return;
    }
  }
}

}  // namespace content
