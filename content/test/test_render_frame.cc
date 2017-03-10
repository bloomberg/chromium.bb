// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include "content/common/navigation_params.h"
#include "content/common/resource_request_body_impl.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_response.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

// static
RenderFrameImpl* TestRenderFrame::CreateTestRenderFrame(
    const RenderFrameImpl::CreateParams& params) {
  return new TestRenderFrame(params);
}

TestRenderFrame::TestRenderFrame(const RenderFrameImpl::CreateParams& params)
    : RenderFrameImpl(params) {
}

TestRenderFrame::~TestRenderFrame() {
}

void TestRenderFrame::Navigate(const CommonNavigationParams& common_params,
                               const StartNavigationParams& start_params,
                               const RequestNavigationParams& request_params) {
  // PlzNavigate
  if (IsBrowserSideNavigationEnabled()) {
    OnCommitNavigation(ResourceResponseHead(), GURL(), common_params,
                       request_params);
  } else {
    OnNavigate(common_params, start_params, request_params);
  }
}

void TestRenderFrame::SwapOut(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state) {
  OnSwapOut(proxy_routing_id, is_loading, replicated_frame_state);
}

void TestRenderFrame::SetEditableSelectionOffsets(int start, int end) {
  OnSetEditableSelectionOffsets(start, end);
}

void TestRenderFrame::ExtendSelectionAndDelete(int before, int after) {
  OnExtendSelectionAndDelete(before, after);
}

void TestRenderFrame::DeleteSurroundingText(int before, int after) {
  OnDeleteSurroundingText(before, after);
}

void TestRenderFrame::DeleteSurroundingTextInCodePoints(int before, int after) {
  OnDeleteSurroundingTextInCodePoints(before, after);
}

void TestRenderFrame::CollapseSelection() {
  OnCollapseSelection();
}

void TestRenderFrame::SetAccessibilityMode(AccessibilityMode new_mode) {
  OnSetAccessibilityMode(new_mode);
}

void TestRenderFrame::SetCompositionFromExistingText(
    int start,
    int end,
    const std::vector<blink::WebCompositionUnderline>& underlines) {
  OnSetCompositionFromExistingText(start, end, underlines);
}

blink::WebNavigationPolicy TestRenderFrame::decidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  if (IsBrowserSideNavigationEnabled() &&
      info.urlRequest.checkForBrowserSideNavigation() &&
      GetWebFrame()->parent() &&
      info.form.isNull()) {
    // RenderViewTest::LoadHTML already disables PlzNavigate for the main frame
    // requests. However if the loaded html has a subframe, the WebURLRequest
    // will be created inside Blink and it won't have this flag set.
    info.urlRequest.setCheckForBrowserSideNavigation(false);
  }
  return RenderFrameImpl::decidePolicyForNavigation(info);
}

}  // namespace content
