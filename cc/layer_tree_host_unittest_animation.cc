// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/layer.h"
#include "cc/test/layer_tree_test_common.h"

namespace cc {
namespace {

class LayerTreeHostAnimationTest : public ThreadedTest {
 public:
  virtual void setupTree() OVERRIDE {
    ThreadedTest::setupTree();
    m_layerTreeHost->rootLayer()->setLayerAnimationDelegate(this);
  }
};

// Makes sure that setNeedsAnimate does not cause the commitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void animate(base::TimeTicks monotonicTime) OVERRIDE {
    // We skip the first commit becasue its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    m_layerTreeHost->setNeedsAnimate();
    // Right now, commitRequested is going to be true, because during
    // beginFrame, we force commitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next didCommit happens, we should
    // verify that commitRequested has gone back to false.
  }

  virtual void didCommit() OVERRIDE {
    if (!num_commits_) {
      EXPECT_FALSE(m_layerTreeHost->commitRequested());
      m_layerTreeHost->setNeedsAnimate();
      EXPECT_FALSE(m_layerTreeHost->commitRequested());
    }

    // Verifies that the setNeedsAnimate we made in ::animate did not
    // trigger commitRequested.
    EXPECT_FALSE(m_layerTreeHost->commitRequested());
    endTest();
    num_commits_++;
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_commits_;
};

TEST_F(LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested,
       runMultiThread) {
  runTest(true);
}

}  // namespace
}  // namespace cc
