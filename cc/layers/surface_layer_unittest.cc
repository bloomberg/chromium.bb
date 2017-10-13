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
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/sequence_surface_reference_factory.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using testing::_;
using testing::Eq;
using testing::ElementsAre;
using testing::SizeIs;

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

class SurfaceLayerTest : public testing::Test {
 public:
  SurfaceLayerTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

  // Synchronizes |layer_tree_host_| and |host_impl_| and pushes surface ids.
  void SynchronizeTrees() {
    TreeSynchronizer::PushLayerProperties(layer_tree_host_.get(),
                                          host_impl_.pending_tree());
    layer_tree_host_->PushSurfaceIdsTo(host_impl_.pending_tree());
  }

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

class MockSurfaceReferenceFactory
    : public viz::SequenceSurfaceReferenceFactory {
 public:
  MockSurfaceReferenceFactory() {}

  // SequenceSurfaceReferenceFactory implementation.
  MOCK_CONST_METHOD1(SatisfySequence, void(const viz::SurfaceSequence&));
  MOCK_CONST_METHOD2(RequireSequence,
                     void(const viz::SurfaceId&, const viz::SurfaceSequence&));

 protected:
  ~MockSurfaceReferenceFactory() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSurfaceReferenceFactory);
};

// Check that one surface can be referenced by multiple LayerTreeHosts, and
// each will create its own viz::SurfaceSequence that's satisfied on
// destruction.
TEST_F(SurfaceLayerTest, MultipleFramesOneSurface) {
  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
  const viz::SurfaceInfo info(
      viz::SurfaceId(kArbitraryFrameSinkId,
                     viz::LocalSurfaceId(1, kArbitraryToken)),
      1.f, gfx::Size(1, 1));
  const viz::SurfaceSequence expected_seq1(viz::FrameSinkId(1, 1), 1u);
  const viz::SurfaceSequence expected_seq2(viz::FrameSinkId(2, 2), 1u);
  const viz::SurfaceId expected_id(kArbitraryFrameSinkId,
                                   viz::LocalSurfaceId(1, kArbitraryToken));

  scoped_refptr<MockSurfaceReferenceFactory> ref_factory =
      new testing::StrictMock<MockSurfaceReferenceFactory>();

  // We are going to set up the SurfaceLayers and LayerTreeHosts. Each layer
  // will require a sequence and no sequence should be satisfied for now.
  EXPECT_CALL(*ref_factory, RequireSequence(Eq(expected_id), Eq(expected_seq1)))
      .Times(1);
  EXPECT_CALL(*ref_factory, RequireSequence(Eq(expected_id), Eq(expected_seq2)))
      .Times(1);
  EXPECT_CALL(*ref_factory, SatisfySequence(_)).Times(0);

  auto layer = SurfaceLayer::Create(ref_factory);
  layer->SetPrimarySurfaceInfo(info);
  layer->SetFallbackSurfaceId(info.id());
  layer_tree_host_->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      viz::FrameSinkId(1, 1));
  layer_tree_host_->SetRootLayer(layer);

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host2 =
      FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_,
                                animation_host2.get());
  auto layer2 = SurfaceLayer::Create(ref_factory);
  layer2->SetPrimarySurfaceInfo(info);
  layer2->SetFallbackSurfaceId(info.id());
  layer_tree_host2->GetSurfaceSequenceGenerator()->set_frame_sink_id(
      viz::FrameSinkId(2, 2));
  layer_tree_host2->SetRootLayer(layer2);

  testing::Mock::VerifyAndClearExpectations(ref_factory.get());

  // Destroy the second LayerTreeHost. The sequence generated by its
  // SurfaceLayer must be satisfied and no new sequences must be required.
  EXPECT_CALL(*ref_factory, SatisfySequence(Eq(expected_seq2))).Times(1);
  layer_tree_host2->SetRootLayer(nullptr);
  layer_tree_host2.reset();
  animation_host2 = nullptr;
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(ref_factory.get());

  // Destroy the first LayerTreeHost. The sequence generated by its
  // SurfaceLayer must be satisfied and no new sequences must be required.
  EXPECT_CALL(*ref_factory, SatisfySequence(expected_seq1)).Times(1);
  EXPECT_CALL(*ref_factory, RequireSequence(_, _)).Times(0);
  layer_tree_host_->SetRootLayer(nullptr);
  layer_tree_host_.reset();
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(ref_factory.get());
}

// This test verifies that SurfaceLayer properties are pushed across to
// SurfaceLayerImpl.
TEST_F(SurfaceLayerTest, PushProperties) {
  // We use a nice mock here because we are not really interested in calls to
  // MockSurfaceReferenceFactory and we don't want warnings printed.
  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory =
      new testing::NiceMock<MockSurfaceReferenceFactory>();

  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create(ref_factory);
  layer_tree_host_->SetRootLayer(layer);
  viz::SurfaceInfo primary_info(
      viz::SurfaceId(kArbitraryFrameSinkId,
                     viz::LocalSurfaceId(1, base::UnguessableToken::Create())),
      1.f, gfx::Size(1, 1));
  layer->SetPrimarySurfaceInfo(primary_info);
  layer->SetFallbackSurfaceId(primary_info.id());
  layer->SetDefaultBackgroundColor(SK_ColorBLUE);
  layer->SetStretchContentToFillBounds(true);

  EXPECT_TRUE(layer_tree_host_->needs_surface_ids_sync());
  EXPECT_EQ(layer_tree_host_->SurfaceLayerIds().size(), 1u);

  // Verify that pending tree has no surface ids already.
  EXPECT_FALSE(host_impl_.pending_tree()->needs_surface_ids_sync());
  EXPECT_EQ(host_impl_.pending_tree()->SurfaceLayerIds().size(), 0u);

  std::unique_ptr<SurfaceLayerImpl> layer_impl =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer->id());
  SynchronizeTrees();

  // Verify that pending tree received the surface id and also has
  // needs_surface_ids_sync set to true as it needs to sync with active tree.
  EXPECT_TRUE(host_impl_.pending_tree()->needs_surface_ids_sync());
  EXPECT_EQ(host_impl_.pending_tree()->SurfaceLayerIds().size(), 1u);

  // Verify we have reset the state on layer tree host.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ids_sync());

  // Verify that the primary and fallback SurfaceInfos are pushed through.
  EXPECT_EQ(primary_info, layer_impl->primary_surface_info());
  EXPECT_EQ(primary_info.id(), layer_impl->fallback_surface_id());
  EXPECT_EQ(SK_ColorBLUE, layer_impl->default_background_color());
  EXPECT_TRUE(layer_impl->stretch_content_to_fill_bounds());

  viz::SurfaceInfo fallback_info(
      viz::SurfaceId(kArbitraryFrameSinkId,
                     viz::LocalSurfaceId(2, base::UnguessableToken::Create())),
      2.f, gfx::Size(10, 10));
  layer->SetFallbackSurfaceId(fallback_info.id());
  layer->SetDefaultBackgroundColor(SK_ColorGREEN);
  layer->SetStretchContentToFillBounds(false);

  // Verify that fallback surface id is not recorded on the layer tree host as
  // surface synchronization is not enabled.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ids_sync());
  EXPECT_EQ(layer_tree_host_->SurfaceLayerIds().size(), 1u);

  SynchronizeTrees();

  EXPECT_EQ(host_impl_.pending_tree()->SurfaceLayerIds().size(), 1u);

  // Verify that the primary viz::SurfaceInfo stays the same and the new
  // fallback viz::SurfaceInfo is pushed through.
  EXPECT_EQ(primary_info, layer_impl->primary_surface_info());
  EXPECT_EQ(fallback_info.id(), layer_impl->fallback_surface_id());
  EXPECT_EQ(SK_ColorGREEN, layer_impl->default_background_color());
  EXPECT_FALSE(layer_impl->stretch_content_to_fill_bounds());
}

// This test verifies the list of surface ids is correct when there are cloned
// surface layers. This emulates the flow of maximize and minimize animations on
// Chrome OS.
TEST_F(SurfaceLayerTest, CheckSurfaceReferencesForClonedLayer) {
  // We use a nice mock here because we are not really interested in calls to
  // MockSurfaceReferenceFactory and we don't want warnings printed.
  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory =
      new testing::NiceMock<MockSurfaceReferenceFactory>();

  const viz::SurfaceId old_surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  const viz::SurfaceInfo old_surface_info(old_surface_id, 1.f, gfx::Size(1, 1));

  // This layer will always contain the old surface id and will be deleted when
  // animation is done.
  scoped_refptr<SurfaceLayer> layer1 = SurfaceLayer::Create(ref_factory);
  layer1->SetLayerTreeHost(layer_tree_host_.get());
  layer1->SetPrimarySurfaceInfo(old_surface_info);
  layer1->SetFallbackSurfaceId(old_surface_info.id());

  // This layer will eventually be switched be switched to show the new surface
  // id and will be retained when animation is done.
  scoped_refptr<SurfaceLayer> layer2 = SurfaceLayer::Create(ref_factory);
  layer2->SetLayerTreeHost(layer_tree_host_.get());
  layer2->SetPrimarySurfaceInfo(old_surface_info);
  layer2->SetFallbackSurfaceId(old_surface_info.id());

  std::unique_ptr<SurfaceLayerImpl> layer_impl1 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer1->id());
  std::unique_ptr<SurfaceLayerImpl> layer_impl2 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer2->id());

  SynchronizeTrees();

  // Verify that only |old_surface_id| is going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(), ElementsAre(old_surface_id));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceLayerIds(),
              ElementsAre(old_surface_id));

  const viz::SurfaceId new_surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));
  const viz::SurfaceInfo new_surface_info(new_surface_id, 1.f, gfx::Size(2, 2));

  // Switch the new layer to use |new_surface_id|.
  layer2->SetPrimarySurfaceInfo(new_surface_info);
  layer2->SetFallbackSurfaceId(new_surface_info.id());

  SynchronizeTrees();

  // Verify that both surface ids are going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(),
              ElementsAre(old_surface_id, new_surface_id));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceLayerIds(),
              ElementsAre(old_surface_id, new_surface_id));

  // Unparent the old layer like it's being destroyed at the end of animation.
  layer1->SetLayerTreeHost(nullptr);

  SynchronizeTrees();

  // Verify that only |new_surface_id| is going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(), ElementsAre(new_surface_id));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceLayerIds(),
              ElementsAre(new_surface_id));

  // Cleanup for destruction.
  layer2->SetLayerTreeHost(nullptr);
}

// This test verifies LayerTreeHost::needs_surface_ids_sync() is correct when
// there are cloned surface layers.
TEST_F(SurfaceLayerTest, CheckNeedsSurfaceIdsSyncForClonedLayers) {
  // We use a nice mock here because we are not really interested in calls to
  // MockSurfaceReferenceFactory and we don't want warnings printed.
  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory =
      new testing::NiceMock<MockSurfaceReferenceFactory>();

  const viz::SurfaceInfo surface_info(
      viz::SurfaceId(kArbitraryFrameSinkId,
                     viz::LocalSurfaceId(1, base::UnguessableToken::Create())),
      1.f, gfx::Size(1, 1));

  scoped_refptr<SurfaceLayer> layer1 = SurfaceLayer::Create(ref_factory);
  layer1->SetLayerTreeHost(layer_tree_host_.get());
  layer1->SetPrimarySurfaceInfo(surface_info);
  layer1->SetFallbackSurfaceId(surface_info.id());

  // Verify the surface id is in SurfaceLayerIds() and needs_surface_ids_sync()
  // is true.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ids_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(), SizeIs(1));

  std::unique_ptr<SurfaceLayerImpl> layer_impl1 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer1->id());
  SynchronizeTrees();

  // After syncchronizing trees verify needs_surface_ids_sync() is false.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ids_sync());

  // Create the second layer that is a clone of the first.
  scoped_refptr<SurfaceLayer> layer2 = SurfaceLayer::Create(ref_factory);
  layer2->SetLayerTreeHost(layer_tree_host_.get());
  layer2->SetPrimarySurfaceInfo(surface_info);
  layer2->SetFallbackSurfaceId(surface_info.id());

  // Verify that after creating the second layer with the same surface id that
  // needs_surface_ids_sync() is still false.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ids_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(), SizeIs(1));

  std::unique_ptr<SurfaceLayerImpl> layer_impl2 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer2->id());
  SynchronizeTrees();

  // Verify needs_surface_ids_sync() is still false after synchronizing trees.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ids_sync());

  // Destroy one of the layers, leaving one layer with the surface id.
  layer1->SetLayerTreeHost(nullptr);

  // Verify needs_surface_ids_sync() is still false.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ids_sync());

  // Destroy the last layer, this should change the set of layer surface ids.
  layer2->SetLayerTreeHost(nullptr);

  // Verify SurfaceLayerIds() is empty and needs_surface_ids_sync() is true.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ids_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceLayerIds(), SizeIs(0));
}

// Check that viz::SurfaceSequence is sent through swap promise.
class SurfaceLayerSwapPromise : public LayerTreeTest {
 public:
  SurfaceLayerSwapPromise()
      : commit_count_(0), sequence_was_satisfied_(false) {}

  void BeginTest() override {
    layer_tree_host()->GetSurfaceSequenceGenerator()->set_frame_sink_id(
        viz::FrameSinkId(1, 1));
    ref_factory_ = new testing::StrictMock<MockSurfaceReferenceFactory>();

    // Create a SurfaceLayer but don't add it to the tree yet. No sequence
    // should be required / satisfied.
    EXPECT_CALL(*ref_factory_, SatisfySequence(_)).Times(0);
    EXPECT_CALL(*ref_factory_, RequireSequence(_, _)).Times(0);
    layer_ = SurfaceLayer::Create(ref_factory_);
    viz::SurfaceInfo info(
        viz::SurfaceId(kArbitraryFrameSinkId,
                       viz::LocalSurfaceId(1, kArbitraryToken)),
        1.f, gfx::Size(1, 1));
    layer_->SetPrimarySurfaceInfo(info);
    layer_->SetFallbackSurfaceId(info.id());
    testing::Mock::VerifyAndClearExpectations(ref_factory_.get());

    // Add the layer to the tree. A sequence must be required.
    viz::SurfaceSequence expected_seq(kArbitraryFrameSinkId, 1u);
    viz::SurfaceId expected_id(kArbitraryFrameSinkId,
                               viz::LocalSurfaceId(1, kArbitraryToken));
    EXPECT_CALL(*ref_factory_, SatisfySequence(_)).Times(0);
    EXPECT_CALL(*ref_factory_,
                RequireSequence(Eq(expected_id), Eq(expected_seq)))
        .Times(1);
    layer_tree_host()->SetRootLayer(layer_);
    testing::Mock::VerifyAndClearExpectations(ref_factory_.get());

    // By the end of the test, the required sequence must be satisfied and no
    // more sequence must be required.
    EXPECT_CALL(*ref_factory_, SatisfySequence(Eq(expected_seq))).Times(1);
    EXPECT_CALL(*ref_factory_, RequireSequence(_, _)).Times(0);

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
        FROM_HERE, base::BindOnce(&SurfaceLayerSwapPromise::ChangeTree,
                                  base::Unretained(this)));
  }

  void AfterTest() override {}

 protected:
  int commit_count_;
  bool sequence_was_satisfied_;
  scoped_refptr<SurfaceLayer> layer_;
  scoped_refptr<Layer> blank_layer_;
  scoped_refptr<MockSurfaceReferenceFactory> ref_factory_;

  const base::UnguessableToken kArbitraryToken =
      base::UnguessableToken::Create();
};

// Check that viz::SurfaceSequence is sent through swap promise.
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
};

SINGLE_AND_MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithDraw);

// Check that viz::SurfaceSequence is sent through swap promise and resolved
// when swap fails.
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
};

MULTI_THREAD_TEST_F(SurfaceLayerSwapPromiseWithoutDraw);

}  // namespace
}  // namespace cc
