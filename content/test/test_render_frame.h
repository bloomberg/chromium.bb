// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_H_

#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"

namespace blink {
class WebHistoryItem;
}

namespace content {

struct CommonNavigationParams;
struct RequestNavigationParams;
struct StartNavigationParams;

// A test class to use in RenderViewTests.
class TestRenderFrame : public RenderFrameImpl {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      const RenderFrameImpl::CreateParams& params);
  ~TestRenderFrame() override;

  const blink::WebHistoryItem& current_history_item() {
    return current_history_item_;
  }

  void Navigate(const CommonNavigationParams& common_params,
                const StartNavigationParams& start_params,
                const RequestNavigationParams& request_params);
  void SwapOut(int proxy_routing_id,
               bool is_loading,
               const FrameReplicationState& replicated_frame_state);
  void SetEditableSelectionOffsets(int start, int end);
  void ExtendSelectionAndDelete(int before, int after);
  void DeleteSurroundingText(int before, int after);
  void DeleteSurroundingTextInCodePoints(int before, int after);
  void CollapseSelection();
  void SetAccessibilityMode(AccessibilityMode new_mode);
  void SetCompositionFromExistingText(
      int start,
      int end,
      const std::vector<blink::WebCompositionUnderline>& underlines);

  blink::WebNavigationPolicy decidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) override;

 private:
  explicit TestRenderFrame(const RenderFrameImpl::CreateParams& params);
  DISALLOW_COPY_AND_ASSIGN(TestRenderFrame);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_H_
