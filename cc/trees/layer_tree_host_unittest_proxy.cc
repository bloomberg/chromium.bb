// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"

#define PROXY_MAIN_THREADED_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, MultiThread) { Run(true); }

// Do common tests for single thread proxy and proxy main in threaded mode.
// TODO(simonhong): Add SINGLE_THREAD_PROXY_TEST_F
#define PROXY_TEST_SCHEDULED_ACTION(TEST_FIXTURE_NAME) \
  PROXY_MAIN_THREADED_TEST_F(TEST_FIXTURE_NAME);

namespace cc {

class ProxyTest : public LayerTreeTest {
 protected:
  ProxyTest() {}
  ~ProxyTest() override {}

  void Run(bool threaded) {
    // We don't need to care about delegating mode.
    bool delegating_renderer = true;

    CompositorMode mode =
        threaded ? CompositorMode::Threaded : CompositorMode::SingleThreaded;
    RunTest(mode, delegating_renderer);
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

class ProxyMainThreaded : public ProxyTest {
 protected:
  ProxyMainThreaded()
      : update_check_layer_(
            FakePictureLayer::Create(layer_settings(), &client_)) {}
  ~ProxyMainThreaded() override {}

  void SetupTree() override {
    layer_tree_host()->SetRootLayer(update_check_layer_);
    ProxyTest::SetupTree();
    client_.set_bounds(update_check_layer_->bounds());
  }

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreaded);
};

class ProxyMainThreadedSetNeedsCommit : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsCommit() {}
  ~ProxyMainThreadedSetNeedsCommit() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsCommit);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsCommit);

class ProxyMainThreadedSetNeedsAnimate : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsAnimate() {}
  ~ProxyMainThreadedSetNeedsAnimate() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsAnimate();

    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(0, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsAnimate);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsAnimate);

class ProxyMainThreadedSetNeedsUpdateLayers : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsUpdateLayers() {}
  ~ProxyMainThreadedSetNeedsUpdateLayers() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsUpdateLayers);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsUpdateLayers);

class ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating
    : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating() {}
  ~ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating() override {}

  void BeginTest() override { proxy()->SetNeedsAnimate(); }

  void WillBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating);

class ProxyMainThreadedSetNeedsCommitWhileAnimating : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsCommitWhileAnimating() {}
  ~ProxyMainThreadedSetNeedsCommitWhileAnimating() override {}

  void BeginTest() override { proxy()->SetNeedsAnimate(); }

  void WillBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsCommitWhileAnimating);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsCommitWhileAnimating);

class ProxyMainThreadedCommitWaitsForActivation : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedCommitWaitsForActivation() : commits_completed_(0) {}
  ~ProxyMainThreadedCommitWaitsForActivation() override {}

  void BeginTest() override { proxy()->SetNeedsCommit(); }

  void ScheduledActionCommit() override {
    switch (commits_completed_) {
      case 0:
        // The first commit does not wait for activation. Verify that the
        // completion event is cleared.
        EXPECT_FALSE(GetProxyImplForTest()->HasCommitCompletionEvent());
        EXPECT_FALSE(GetProxyImplForTest()->GetNextCommitWaitsForActivation());
        break;
      case 1:
        // The second commit should be held until activation.
        EXPECT_TRUE(GetProxyImplForTest()->HasCommitCompletionEvent());
        EXPECT_TRUE(GetProxyImplForTest()->GetNextCommitWaitsForActivation());
        break;
      case 2:
        // The third commit should not wait for activation.
        EXPECT_FALSE(GetProxyImplForTest()->HasCommitCompletionEvent());
        EXPECT_FALSE(GetProxyImplForTest()->GetNextCommitWaitsForActivation());
    }
  }

  void DidActivateSyncTree() override {
    // The next_commit_waits_for_activation should have been cleared after the
    // sync tree is activated.
    EXPECT_FALSE(GetProxyImplForTest()->GetNextCommitWaitsForActivation());
  }

  void DidCommit() override {
    switch (commits_completed_) {
      case 0:
        // The first commit has been completed. Set next commit waits for
        // activation and start another commit.
        commits_completed_++;
        proxy()->SetNextCommitWaitsForActivation();
        proxy()->SetNeedsCommit();
      case 1:
        // Start another commit to verify that this is not held until
        // activation.
        commits_completed_++;
        proxy()->SetNeedsCommit();
      case 2:
        commits_completed_++;
        EndTest();
    }
  }

  void AfterTest() override { EXPECT_EQ(3, commits_completed_); }

 private:
  int commits_completed_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedCommitWaitsForActivation);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedCommitWaitsForActivation);

}  // namespace cc
