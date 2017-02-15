// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <set>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "cc/surfaces/surface_info.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class SurfaceLayerTest : public testing::Test {
 public:
  SurfaceLayerTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_host_->SetViewportSize(gfx::Size(10, 10));
    host_impl_.CreatePendingTree();
  }

  void TearDown() override {
    if (layer_tree_host_) {
      layer_tree_host_->SetRootLayer(nullptr);
      layer_tree_host_ = nullptr;
    }
  }

  FakeLayerTreeHostClient fake_client_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostImpl host_impl_;
};

class TestSurfaceReferenceFactory : public SequenceSurfaceReferenceFactory {
 protected:
  void SatisfySequence(const SurfaceSequence& seq) const override {
    *out_seq_ = seq;
  }

  void RequireSequence(const SurfaceId& id,
                       const SurfaceSequence& seq) const override {
    *out_id_ = id;
    out_set_->insert(seq);
  }

 public:
  TestSurfaceReferenceFactory(SurfaceSequence* out_seq,
                              SurfaceId* out_id,
                              std::set<SurfaceSequence>* out_set)
      : out_seq_(out_seq), out_id_(out_id), out_set_(out_set) {}

 protected:
  ~TestSurfaceReferenceFactory() override = default;

 private:
  SurfaceSequence* out_seq_;
  SurfaceId* out_id_;
  std::set<SurfaceSequence>* out_set_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceReferenceFactory);
};

// Check that one surface can be referenced by multiple LayerTreeHosts, and
// each will create its own SurfaceSequence that's satisfied on destruction.
TEST_F(SurfaceLayerTest, MultipleFramesOneSurface) {
  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
  SurfaceSequence blank_change;  // Receives sequence if commit doesn't happen.

  SurfaceId required_id;
  std::set<SurfaceSequence> required_seq;
  scoped_refptr<SurfaceReferenceFactory> ref_factory =
      new TestSurfaceReferenceFactory(&blank_change, &required_id,
                                      &required_seq);
  auto layer = SurfaceLayer::Create(ref_factory);
  SurfaceInfo info(
      SurfaceId(kArbitraryFrameSinkId, LocalSurfaceId(1, kArbitraryToken)), 1.f,
      gfx::Size(1, 1));
  layer->SetPrimarySurfaceInfo(info);
  layer_tree_host_->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      FrameSinkId(1, 1));
  layer_tree_host_->SetRootLayer(layer);

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host2 =
      FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_,
                                animation_host2.get());
  auto layer2 = SurfaceLayer::Create(std::move(ref_factory));
  layer2->SetPrimarySurfaceInfo(info);
  layer_tree_host2->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      FrameSinkId(2, 2));
  layer_tree_host2->SetRootLayer(layer2);

  // Layers haven't been removed, so no sequence should be satisfied.
  EXPECT_FALSE(blank_change.is_valid());

  SurfaceSequence expected1(FrameSinkId(1, 1), 1u);
  SurfaceSequence expected2(FrameSinkId(2, 2), 1u);

  layer_tree_host2->SetRootLayer(nullptr);
  layer_tree_host2.reset();
  animation_host2 = nullptr;

  // Layer was removed so sequence from second LayerTreeHost should be
  // satisfied.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(blank_change == expected2);

  // Set of sequences that need to be satisfied should include sequences from
  // both trees.
  EXPECT_TRUE(required_id == SurfaceId(kArbitraryFrameSinkId,
                                       LocalSurfaceId(1, kArbitraryToken)));
  EXPECT_EQ(2u, required_seq.size());
  EXPECT_TRUE(required_seq.count(expected1));
  EXPECT_TRUE(required_seq.count(expected2));

  layer_tree_host_->SetRootLayer(nullptr);
  layer_tree_host_.reset();

  // Layer was removed so sequence from first LayerTreeHost should be
  // satisfied.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(blank_change == expected1);

  // No more SurfaceSequences should have been generated that need to have be
  // satisfied.
  EXPECT_EQ(2u, required_seq.size());
}

// This test verifies that the primary and fallback SurfaceInfo are pushed
// across from SurfaceLayer to SurfaceLayerImpl.
TEST_F(SurfaceLayerTest, SurfaceInfoPushProperties) {
  SurfaceSequence blank_change;
  SurfaceId required_id;
  std::set<SurfaceSequence> required_sequences;
  scoped_refptr<SurfaceReferenceFactory> ref_factory =
      new TestSurfaceReferenceFactory(&blank_change, &required_id,
                                      &required_sequences);

  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create(ref_factory);
  layer_tree_host_->SetRootLayer(layer);
  SurfaceInfo primary_info(
      SurfaceId(kArbitraryFrameSinkId,
                LocalSurfaceId(1, base::UnguessableToken::Create())),
      1.f, gfx::Size(1, 1));
  layer->SetPrimarySurfaceInfo(primary_info);

  std::unique_ptr<SurfaceLayerImpl> layer_impl =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer->id());
  layer->PushPropertiesTo(layer_impl.get());

  // Verify tha the primary SurfaceInfo is pushed through and that there is
  // no valid fallback SurfaceInfo.
  EXPECT_EQ(primary_info, layer_impl->primary_surface_info());
  EXPECT_EQ(SurfaceInfo(), layer_impl->fallback_surface_info());

  SurfaceInfo fallback_info(
      SurfaceId(kArbitraryFrameSinkId,
                LocalSurfaceId(2, base::UnguessableToken::Create())),
      2.f, gfx::Size(10, 10));
  layer->SetFallbackSurfaceInfo(fallback_info);
  layer->PushPropertiesTo(layer_impl.get());

  // Verify that the primary SurfaceInfo stays the same and the new fallback
  // SurfaceInfo is pushed through.
  EXPECT_EQ(primary_info, layer_impl->primary_surface_info());
  EXPECT_EQ(fallback_info, layer_impl->fallback_surface_info());
}

// Check that SurfaceSequence is sent through swap promise.
class SurfaceLayerSwapPromise : public LayerTreeTest {
 public:
  SurfaceLayerSwapPromise()
      : commit_count_(0), sequence_was_satisfied_(false) {}

  void BeginTest() override {
    layer_tree_host()->GetSurfaceSequenceGenerator()->set_frame_sink_id(
        FrameSinkId(1, 1));
    layer_ = SurfaceLayer::Create(new TestSurfaceReferenceFactory(
        &satisfied_sequence_, &required_id_, &required_set_));
    SurfaceInfo info(
        SurfaceId(kArbitraryFrameSinkId, LocalSurfaceId(1, kArbitraryToken)),
        1.f, gfx::Size(1, 1));
    layer_->SetPrimarySurfaceInfo(info);

    // Layer hasn't been added to tree so no SurfaceSequence generated yet.
    EXPECT_EQ(0u, required_set_.size());

    layer_tree_host()->SetRootLayer(layer_);

    // Should have SurfaceSequence from first tree.
    SurfaceSequence expected(kArbitraryFrameSinkId, 1u);
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalSurfaceId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    EXPECT_TRUE(required_set_.count(expected));

    gfx::Size bounds(100, 100);
    layer_tree_host()->SetViewportSize(bounds);

    blank_layer_ = SolidColorLayer::Create();
    blank_layer_->SetIsDrawable(true);
    blank_layer_->SetBounds(gfx::Size(10, 10));

    PostSetNeedsCommitToMainThread();
  }

  virtual void ChangeTree() = 0;

  void DidCommitAndDrawFrame() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SurfaceLayerSwapPromise::ChangeTree,
                              base::Unretained(this)));
  }

 protected:
  int commit_count_;
  bool sequence_was_satisfied_;
  scoped_refptr<SurfaceLayer> layer_;
  scoped_refptr<Layer> blank_layer_;
  SurfaceSequence satisfied_sequence_;

  SurfaceId required_id_;
  std::set<SurfaceSequence> required_set_;
  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
};

// Check that SurfaceSequence is sent through swap promise.
class SurfaceLayerSwapPromiseWithDraw : public SurfaceLayerSwapPromise {
 public:
  void ChangeTree() override {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Remove SurfaceLayer from tree to cause SwapPromise to be created.
        layer_tree_host()->SetRootLayer(blank_layer_);
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void AfterTest() override {
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalSurfaceId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    EXPECT_TRUE(satisfied_sequence_ ==
                SurfaceSequence(kArbitraryFrameSinkId, 1u));
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithDraw);

// Check that SurfaceSequence is sent through swap promise and resolved when
// swap fails.
class SurfaceLayerSwapPromiseWithoutDraw : public SurfaceLayerSwapPromise {
 public:
  SurfaceLayerSwapPromiseWithoutDraw() : SurfaceLayerSwapPromise() {}

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    return DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
  }

  void ChangeTree() override {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Remove SurfaceLayer from tree to cause SwapPromise to be created.
        layer_tree_host()->SetRootLayer(blank_layer_);
        break;
      case 2:
        layer_tree_host()->SetNeedsCommit();
        break;
      default:
        EndTest();
        break;
    }
  }

  void AfterTest() override {
    EXPECT_TRUE(required_id_ == SurfaceId(kArbitraryFrameSinkId,
                                          LocalSurfaceId(1, kArbitraryToken)));
    EXPECT_EQ(1u, required_set_.size());
    EXPECT_TRUE(satisfied_sequence_ ==
                SurfaceSequence(kArbitraryFrameSinkId, 1u));
  }
};

MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithoutDraw);

}  // namespace
}  // namespace cc
