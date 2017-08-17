// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class RenderWidgetHostViewChildFrameTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CheckScreenWidth(RenderFrameHost* render_frame_host) {
    int width;
    ExecuteScriptAndGetValue(render_frame_host, "window.screen.width")
        ->GetAsInteger(&width);
    EXPECT_EQ(expected_screen_width_, width);
  }

  void set_expected_screen_width(int width) { expected_screen_width_ = width; }

 private:
  int expected_screen_width_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

// Tests that the screen is properly reflected for RWHVChildFrame.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest, Screen) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  int main_frame_screen_width = 0;
  ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                           "window.screen.width")
      ->GetAsInteger(&main_frame_screen_width);
  set_expected_screen_width(main_frame_screen_width);
  EXPECT_FALSE(main_frame_screen_width == 0);

  shell()->web_contents()->ForEachFrame(
      base::Bind(&RenderWidgetHostViewChildFrameTest::CheckScreenWidth,
                 base::Unretained(this)));
}

// Test that auto-resize sizes in the top frame are propagated to OOPIF
// RenderWidgetHostViews. See https://crbug.com/726743.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest,
                       ChildFrameAutoResizeUpdate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  root->current_frame_host()->render_view_host()->EnableAutoResize(
      gfx::Size(0, 0), gfx::Size(100, 100));

  RenderWidgetHostView* rwhv =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost()->GetView();

  // Fake an auto-resize update from the parent renderer.
  int routing_id =
      root->current_frame_host()->GetRenderWidgetHost()->GetRoutingID();
  ViewHostMsg_UpdateRect_Params params;
  params.view_size = gfx::Size(75, 75);
  params.flags = 0;
  root->current_frame_host()->GetRenderWidgetHost()->OnMessageReceived(
      ViewHostMsg_UpdateRect(routing_id, params));

  // RenderWidgetHostImpl has delayed auto-resize processing. Yield here to
  // let it complete.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  // The child frame's RenderWidgetHostView should now use the auto-resize value
  // for its visible viewport.
  EXPECT_EQ(gfx::Size(75, 75), rwhv->GetVisibleViewportSize());
}

// A class to filter RequireSequence and SatisfySequence messages sent from
// an embedding renderer for its child's Surfaces.
class SurfaceRefMessageFilter : public BrowserMessageFilter {
 public:
  SurfaceRefMessageFilter()
      : BrowserMessageFilter(FrameMsgStart),
        require_message_loop_runner_(new content::MessageLoopRunner),
        satisfy_message_loop_runner_(new content::MessageLoopRunner),
        satisfy_received_(false),
        require_received_first_(false) {}

  void WaitForRequire() { require_message_loop_runner_->Run(); }

  void WaitForSatisfy() { satisfy_message_loop_runner_->Run(); }

  bool require_received_first() { return require_received_first_; }

 protected:
  ~SurfaceRefMessageFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(SurfaceRefMessageFilter, message)
      IPC_MESSAGE_HANDLER(FrameHostMsg_RequireSequence, OnRequire)
      IPC_MESSAGE_HANDLER(FrameHostMsg_SatisfySequence, OnSatisfy)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  void OnRequire(const viz::SurfaceId& id,
                 const viz::SurfaceSequence sequence) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SurfaceRefMessageFilter::OnRequireOnUI, this));
  }

  void OnRequireOnUI() {
    if (!satisfy_received_)
      require_received_first_ = true;
    require_message_loop_runner_->Quit();
  }

  void OnSatisfy(const viz::SurfaceSequence sequence) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SurfaceRefMessageFilter::OnSatisfyOnUI, this));
  }

  void OnSatisfyOnUI() {
    satisfy_received_ = true;
    satisfy_message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> require_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> satisfy_message_loop_runner_;
  bool satisfy_received_;
  bool require_received_first_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceRefMessageFilter);
};

// Test that when a child frame submits its first compositor frame, the
// embedding renderer process properly acquires and releases references to the
// new Surface. See https://crbug.com/701175.
// TODO(crbug.com/676384): Delete test with the rest of SurfaceSequence code.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest,
                       DISABLED_ChildFrameSurfaceReference) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  scoped_refptr<SurfaceRefMessageFilter> filter = new SurfaceRefMessageFilter();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  NavigateFrameToURL(root->child_at(0), foo_url);

  // If one of these messages isn't received, this test times out.
  filter->WaitForRequire();
  filter->WaitForSatisfy();

  EXPECT_TRUE(filter->require_received_first());
}

}  // namespace content
