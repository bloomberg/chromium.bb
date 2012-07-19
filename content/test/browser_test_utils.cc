// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/public/test/test_launcher.h"

namespace {

// Number of times to repost a Quit task so that the MessageLoop finishes up
// pending tasks and tasks posted by those pending tasks without risking the
// potential hang behavior of MessageLoop::QuitWhenIdle.
// The criteria for choosing this number: it should be high enough to make the
// quit act like QuitWhenIdle, while taking into account that any page which is
// animating may be rendering another frame for each quit deferral. For an
// animating page, the potential delay to quitting the RunLoop would be
// kNumQuitDeferrals * frame_render_time. Some perf tests run slow, such as
// 200ms/frame.
static const int kNumQuitDeferrals = 10;

static void DeferredQuitRunLoop(const base::Closure& quit_task,
                                int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    quit_task.Run();
  } else {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&DeferredQuitRunLoop, quit_task, num_quit_deferrals - 1));
  }
}

}

namespace content {

void RunThisRunLoop(base::RunLoop* run_loop) {
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

  test_launcher::TestLauncherDelegate* delegate =
      test_launcher::GetCurrentTestLauncherDelegate();
  // TODO(jam): the null check shouldn't be needed, since this code should only
  // be called from browser_tests. Unfortunately some unittests are calling this
  // code.
  if (delegate)
    delegate->PreRunMessageLoop(run_loop);
  run_loop->Run();
  if (delegate)
    delegate->PostRunMessageLoop();
}

base::Closure GetQuitTaskForRunLoop(base::RunLoop* run_loop) {
  return base::Bind(&DeferredQuitRunLoop, run_loop->QuitClosure(),
                    kNumQuitDeferrals);
}

}  // namespace content
