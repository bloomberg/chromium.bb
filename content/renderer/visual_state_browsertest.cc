// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/render_frame_impl.h"
#include "content/shell/browser/shell.h"

namespace content {

class CommitObserver : public RenderViewObserver {
 public:
  CommitObserver(RenderView* render_view, const base::Closure& quit_closure)
      : RenderViewObserver(render_view),
        quit_closure_(quit_closure),
        commit_count_(0) {}

  void DidCommitCompositorFrame() override {
    commit_count_++;
    quit_closure_.Run();
  }

  int GetCommitCount() { return commit_count_; }

 private:
  base::Closure quit_closure_;
  int commit_count_;
};

class VisualStateTest : public ContentBrowserTest {
 public:
  VisualStateTest() : callback_count_(0) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void WaitForCommit(int routing_id) {
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
    CommitObserver observer(GetRenderView(routing_id), runner->QuitClosure());
    runner->Run();
    EXPECT_EQ(1, observer.GetCommitCount());
  }

  void AssertIsIdle() {
    ASSERT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
  }

  void InvokeVisualStateCallback(bool result) {
    EXPECT_TRUE(result);
    callback_count_++;
  }

  int GetCallbackCount() { return callback_count_; }

 private:
  RenderView* GetRenderView(int routing_id) {
    return RenderView::FromRoutingID(routing_id);
  }

  int callback_count_;
};

// This test verifies that visual state callbacks do not deadlock. In other
// words, the visual state callback should be received even if there are no
// pending updates or commits.
IN_PROC_BROWSER_TEST_F(VisualStateTest, CallbackDoesNotDeadlock) {
  // This test relies on the fact that loading "about:blank" only requires a
  // single commit. We first load "about:blank" and wait for this single
  // commit. At that point we know that the page has stabilized and no
  // further commits are expected. We then insert a visual state callback
  // and verify that this causes an additional commit in order to deliver
  // the callback.
  // Unfortunately, if loading "about:blank" changes and starts requiring
  // two commits then this test will prove nothing. We could detect this
  // with a high level of confidence if we used a timeout, but that's
  // discouraged (see https://codereview.chromium.org/939673002).
  NavigateToURL(shell(), GURL("about:blank"));

  // Wait for the commit corresponding to the load.
  PostTaskToInProcessRendererAndWait(
      base::Bind(&VisualStateTest::WaitForCommit, base::Unretained(this),
                 shell()->web_contents()->GetRoutingID()));

  // Try our best to check that there are no pending updates or commits.
  PostTaskToInProcessRendererAndWait(
      base::Bind(&VisualStateTest::AssertIsIdle, base::Unretained(this)));

  // Insert a visual state callback.
  shell()->web_contents()->GetMainFrame()->InsertVisualStateCallback(base::Bind(
      &VisualStateTest::InvokeVisualStateCallback, base::Unretained(this)));

  // Verify that the callback is invoked and a new commit completed.
  PostTaskToInProcessRendererAndWait(
      base::Bind(&VisualStateTest::WaitForCommit, base::Unretained(this),
                 shell()->web_contents()->GetRoutingID()));
  EXPECT_EQ(1, GetCallbackCount());
}

}  // namespace content
