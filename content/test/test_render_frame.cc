// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

class MockFrameHost : public mojom::FrameHost {
 public:
  MockFrameHost() {}
  ~MockFrameHost() override = default;

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams() {
    return std::move(last_commit_params_);
  }

 protected:
  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr,
                       CreateNewWindowCallback) override {
    NOTREACHED() << "We should never dispatch to the service side signature.";
  }

  bool CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       mojom::CreateNewWindowReplyPtr* reply) override {
    *reply = mojom::CreateNewWindowReply::New();
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    mock_render_thread->OnCreateWindow(*params, reply->get());
    return true;
  }

  void IssueKeepAliveHandle(mojom::KeepAliveHandleRequest request) override {}

  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params)
      override {
    last_commit_params_ = std::move(params);
  }

 private:
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
      last_commit_params_;

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
}

TestRenderFrame::~TestRenderFrame() {}

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
    const std::vector<blink::WebImeTextSpan>& ime_text_spans) {
  OnSetCompositionFromExistingText(start, end, ime_text_spans);
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
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::MakeUnique<WebURLLoaderImpl>(
      nullptr, base::ThreadTaskRunnerHandle::Get(), nullptr);
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
TestRenderFrame::TakeLastCommitParams() {
  return mock_frame_host_->TakeLastCommitParams();
}

mojom::FrameHost* TestRenderFrame::GetFrameHost() {
  // Need to mock this interface directly without going through a binding,
  // otherwise calling its sync methods could lead to a deadlock.
  //
  // Imagine the following sequence of events take place:
  //
  //   1.) GetFrameHost() called for the first time
  //   1.1.) GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_ptr_)
  //   1.1.1) ... plumbing ...
  //   1.1.2) Task posted to bind the request end to the Mock implementation
  //   1.2) The interface pointer end is returned to the caller
  //   2.) GetFrameHost()->CreateNewWindow(...) sync method invoked
  //   2.1.) Mojo sync request sent
  //   2.2.) Waiting for sync response while dispatching incoming sync requests
  //
  // Normally the sync Mojo request would be processed in 2.2. However, the
  // implementation is not yet bound at that point, and will never be, because
  // only sync IPCs are dispatched by 2.2, not posted tasks. So the sync request
  // is never dispatched, the response never arrives.
  //
  // Because the first invocation to GetFrameHost() may come while we are inside
  // a message loop already, pumping messags before 1.2 would constitute a
  // nested message loop and is therefore undesired.
  return mock_frame_host_.get();
}

}  // namespace content
