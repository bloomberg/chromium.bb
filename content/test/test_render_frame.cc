// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/web_url_loader_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/mock_render_thread.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

class MockFrameHost : public mojom::FrameHost {
 public:
  MockFrameHost() : binding_(this) {}
  ~MockFrameHost() override = default;

  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override {
    mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New();
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    mock_render_thread->OnCreateWindow(*params, reply.get());
    std::move(callback).Run(std::move(reply));
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojom::FrameHostAssociatedRequest(std::move(handle)));
  }

 private:
  mojo::AssociatedBinding<mojom::FrameHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameHost);
};

// static
RenderFrameImpl* TestRenderFrame::CreateTestRenderFrame(
    const RenderFrameImpl::CreateParams& params) {
  return new TestRenderFrame(params);
}

TestRenderFrame::TestRenderFrame(const RenderFrameImpl::CreateParams& params)
    : RenderFrameImpl(params),
      mock_frame_host_(base::MakeUnique<MockFrameHost>()) {
  GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      mojom::FrameHost::Name_,
      base::Bind(&MockFrameHost::Bind,
                 base::Unretained(mock_frame_host_.get())));
}

TestRenderFrame::~TestRenderFrame() {
}

void TestRenderFrame::Navigate(const CommonNavigationParams& common_params,
                               const StartNavigationParams& start_params,
                               const RequestNavigationParams& request_params) {
  // PlzNavigate
  if (IsBrowserSideNavigationEnabled()) {
    OnCommitNavigation(ResourceResponseHead(), GURL(),
                       FrameMsg_CommitDataNetworkService_Params(),
                       common_params, request_params);
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

void TestRenderFrame::SetAccessibilityMode(ui::AXMode new_mode) {
  OnSetAccessibilityMode(new_mode);
}

void TestRenderFrame::SetCompositionFromExistingText(
    int start,
    int end,
    const std::vector<blink::WebCompositionUnderline>& underlines) {
  OnSetCompositionFromExistingText(start, end, underlines);
}

blink::WebNavigationPolicy TestRenderFrame::DecidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  if (IsBrowserSideNavigationEnabled() &&
      info.url_request.CheckForBrowserSideNavigation() &&
      GetWebFrame()->Parent() && info.form.IsNull()) {
    // RenderViewTest::LoadHTML already disables PlzNavigate for the main frame
    // requests. However if the loaded html has a subframe, the WebURLRequest
    // will be created inside Blink and it won't have this flag set.
    info.url_request.SetCheckForBrowserSideNavigation(false);
  }
  return RenderFrameImpl::DecidePolicyForNavigation(info);
}

std::unique_ptr<blink::WebURLLoader> TestRenderFrame::CreateURLLoader(
    const blink::WebURLRequest& request,
    base::SingleThreadTaskRunner* task_runner) {
  return base::MakeUnique<WebURLLoaderImpl>(
      nullptr, base::ThreadTaskRunnerHandle::Get(), nullptr);
}

mojom::FrameHostAssociatedPtr TestRenderFrame::GetFrameHost() {
  mojom::FrameHostAssociatedPtr ptr = RenderFrameImpl::GetFrameHost();

  // Needed to ensure no deadlocks when waiting for sync IPC.
  ptr.FlushForTesting();
  return ptr;
}

}  // namespace content
