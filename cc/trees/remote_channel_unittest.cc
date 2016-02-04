// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/begin_frame_args_test.h"
#include "cc/test/layer_tree_test.h"

namespace cc {

class RemoteChannelTest : public LayerTreeTest {
 protected:
  RemoteChannelTest() : calls_received_(0) {}

  ~RemoteChannelTest() override {}

  void BeginTest() override {
    DCHECK(IsRemoteTest());
    BeginChannelTest();
  }
  virtual void BeginChannelTest() {}

  int calls_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteChannelTest);
};

class RemoteChannelTestInitializationAndShutdown : public RemoteChannelTest {
  void SetVisibleOnImpl(bool visible) override { calls_received_++; }

  void RequestNewOutputSurface() override {
    LayerTreeTest::RequestNewOutputSurface();
    calls_received_++;
  }

  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override {
    calls_received_++;
  }

  void DidInitializeOutputSurface() override {
    calls_received_++;
    EndTest();
  }

  void FinishGLOnImpl() override { calls_received_++; }

  // On initialization and shutdown, each of the above calls should happen only
  // once.
  void AfterTest() override { EXPECT_EQ(5, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestInitializationAndShutdown);

class RemoteChannelTestMainThreadStoppedFlinging : public RemoteChannelTest {
  void BeginChannelTest() override { proxy()->MainThreadHasStoppedFlinging(); }

  void MainThreadHasStoppedFlingingOnImpl() override {
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestMainThreadStoppedFlinging);

class RemoteChannelTestDeferCommits : public RemoteChannelTest {
  void BeginChannelTest() override { DispatchSetDeferCommits(true); }

  void SetDeferCommitsOnImpl(bool defer_commits) override {
    EXPECT_TRUE(defer_commits);
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestDeferCommits);

class RemoteChannelTestNeedsRedraw : public RemoteChannelTest {
  void BeginChannelTest() override {
    damaged_rect_.set_width(4);
    damaged_rect_.set_height(5);
    layer_tree_host()->SetNeedsRedrawRect(damaged_rect_);
  }

  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override {
    EXPECT_EQ(damage_rect, damaged_rect_);
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }

  gfx::Rect damaged_rect_;
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestNeedsRedraw);

class RemoteChannelTestReleaseOutputSurface : public RemoteChannelTest {
  void DidInitializeOutputSurface() override {
    SetVisibleOnLayerTreeHost(false);
    ReleaseOutputSurfaceOnLayerTreeHost();
  }

  void ReleaseOutputSurfaceOnImpl() override {
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestReleaseOutputSurface);

class RemoteChannelTestCommit : public RemoteChannelTest {
  void BeginChannelTest() override {
    layer_tree_host()->SetViewportSize(viewport_size_);
    PostSetNeedsCommitToMainThread();
  }

  void SetNeedsCommitOnImpl() override { EXPECT_EQ(0, calls_received_++); }

  void ReceivedBeginMainFrame() override { EXPECT_EQ(1, calls_received_++); }

  void StartCommitOnImpl() override { EXPECT_EQ(2, calls_received_++); }

  void DidCommitAndDrawFrame() override { EXPECT_EQ(3, calls_received_++); }

  void DidCompleteSwapBuffers() override {
    EXPECT_EQ(4, calls_received_++);
    EndTest();
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // Ensure that we serialized and deserialized the LayerTreeHost for the
    // commit.
    EXPECT_EQ(viewport_size_, host_impl->device_viewport_size());
  }

  void ScheduledActionSendBeginMainFrame() override {
    last_args_sent_ = GetProxyImplForTest()->last_begin_frame_args();
  }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    last_args_received_ = args;
  }

  void AfterTest() override {
    EXPECT_EQ(5, calls_received_);

    // Ensure that we serialized and deserialized the
    // BeginMainFrameAndCommitState. While the last_args_received_ will be set
    // on the impl thread, it is safe to read it here since the impl thread has
    // been destroyed now.
    EXPECT_EQ(last_args_sent_, last_args_received_);
  }

  const gfx::Size viewport_size_ = gfx::Size(5, 3);
  BeginFrameArgs last_args_sent_;
  BeginFrameArgs last_args_received_;
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestCommit);

class RemoteChannelTestBeginMainFrameAborted : public RemoteChannelTest {
  void BeginChannelTest() override { PostSetNeedsCommitToMainThread(); }

  void ScheduledActionWillSendBeginMainFrame() override {
    PostSetDeferCommitsToMainThread(true);
  }

  void BeginMainFrameAbortedOnImpl(CommitEarlyOutReason reason) override {
    EXPECT_EQ(reason, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestBeginMainFrameAborted);

}  // namespace cc
