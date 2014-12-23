// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/thread_proxy.h"

#define THREAD_PROXY_NO_IMPL_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, Run_MainThreadPaint) {     \
    Run(true, false);                                  \
  }

#define THREAD_PROXY_TEST_F(TEST_FIXTURE_NAME)    \
  THREAD_PROXY_NO_IMPL_TEST_F(TEST_FIXTURE_NAME); \
  TEST_F(TEST_FIXTURE_NAME, Run_ImplSidePaint) {  \
    Run(true, true);                              \
  }

// Do common tests for single thread proxy and thread proxy.
// TODO(simonhong): Add SINGLE_THREAD_PROXY_TEST_F
#define PROXY_TEST_SCHEDULED_ACTION(TEST_FIXTURE_NAME) \
  THREAD_PROXY_TEST_F(TEST_FIXTURE_NAME);

namespace cc {

class ProxyTest : public LayerTreeTest {
 protected:
  ProxyTest() {}
  ~ProxyTest() override {}

  void Run(bool threaded, bool impl_side_painting) {
    // We don't need to care about delegating mode.
    bool delegating_renderer = true;

    RunTest(threaded, delegating_renderer, impl_side_painting);
  }

  void BeginTest() override {}
  void AfterTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyTest);
};

class ProxyTestScheduledActionsBasic : public ProxyTest {
 protected:
  void BeginTest() override { proxy()->SetNeedsCommit(); }

  void ScheduledActionBeginOutputSurfaceCreation() override {
    EXPECT_EQ(0, action_phase_++);
  }

  void ScheduledActionSendBeginMainFrame() override {
    EXPECT_EQ(1, action_phase_++);
  }

  void ScheduledActionCommit() override { EXPECT_EQ(2, action_phase_++); }

  void ScheduledActionDrawAndSwapIfPossible() override {
    EXPECT_EQ(3, action_phase_++);
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(4, action_phase_); }

  ProxyTestScheduledActionsBasic() : action_phase_(0) {
  }
  ~ProxyTestScheduledActionsBasic() override {}

 private:
  int action_phase_;

  DISALLOW_COPY_AND_ASSIGN(ProxyTestScheduledActionsBasic);
};

PROXY_TEST_SCHEDULED_ACTION(ProxyTestScheduledActionsBasic);

class ThreadProxyTest : public ProxyTest {
 protected:
  ThreadProxyTest() {}
  ~ThreadProxyTest() override {}

  const ThreadProxy::MainThreadOnly& ThreadProxyMainOnly() const {
    DCHECK(proxy());
    DCHECK(proxy()->HasImplThread());
    return static_cast<const ThreadProxy*>(proxy())->main();
  }

  const ThreadProxy::CompositorThreadOnly& ThreadProxyImplOnly() const {
    DCHECK(proxy());
    DCHECK(proxy()->HasImplThread());
    return static_cast<const ThreadProxy*>(proxy())->impl();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadProxyTest);
};

class ThreadProxyTestSetNeedsCommit : public ThreadProxyTest {
 protected:
  ThreadProxyTestSetNeedsCommit() {}
  ~ThreadProxyTestSetNeedsCommit() override {}

  void BeginTest() override {
    EXPECT_FALSE(ThreadProxyMainOnly().commit_requested);
    EXPECT_FALSE(ThreadProxyMainOnly().commit_request_sent_to_impl_thread);

    proxy()->SetNeedsCommit();

    EXPECT_TRUE(ThreadProxyMainOnly().commit_requested);
    EXPECT_TRUE(ThreadProxyMainOnly().commit_request_sent_to_impl_thread);
  }

  void DidBeginMainFrame() override {
    EXPECT_FALSE(ThreadProxyMainOnly().commit_requested);
    EXPECT_FALSE(ThreadProxyMainOnly().commit_request_sent_to_impl_thread);

    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadProxyTestSetNeedsCommit);
};

THREAD_PROXY_TEST_F(ThreadProxyTestSetNeedsCommit);

}  // namespace cc
