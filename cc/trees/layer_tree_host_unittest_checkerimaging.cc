// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/completion_event.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

class LayerTreeHostCheckerImagingTest : public LayerTreeTest {
 public:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }
  void AfterTest() override {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_checker_imaging = true;
  }

  void SetupTree() override {
    // Set up a content client which creates the following tiling, x denoting
    // the image to checker:
    // |---|---|---|---|
    // | x | x |   |   |
    // |---|---|---|---|
    // | x | x |   |   |
    // |---|---|---|---|
    gfx::Size layer_size(1000, 500);
    content_layer_client_.set_bounds(layer_size);
    content_layer_client_.set_fill_with_nonsolid_color(true);
    sk_sp<SkImage> checkerable_image =
        CreateDiscardableImage(gfx::Size(450, 450));
    content_layer_client_.add_draw_image(checkerable_image, gfx::Point(0, 0),
                                         PaintFlags());

    layer_tree_host()->SetRootLayer(
        FakePictureLayer::Create(&content_layer_client_));
    layer_tree_host()->root_layer()->SetBounds(layer_size);
    LayerTreeTest::SetupTree();
  }

  void FlushImageDecodeTasks() {
    CompletionEvent completion_event;
    image_worker_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce([](CompletionEvent* event) { event->Signal(); },
                       base::Unretained(&completion_event)));
    completion_event.Wait();
  }

 private:
  // Accessed only on the main thread.
  FakeContentLayerClient content_layer_client_;
};

class LayerTreeHostCheckerImagingTestMergeWithMainFrame
    : public LayerTreeHostCheckerImagingTest {
  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // The first commit has happened, invalidate a tile outside the region
      // for the image to ensure that the final invalidation on the pending
      // tree is the union of this and impl-side invalidation.
      layer_tree_host()->root_layer()->SetNeedsDisplayRect(
          gfx::Rect(600, 0, 50, 500));
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void ReadyToCommitOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_of_commits_ == 1) {
      // Send the blocked invalidation request before notifying that we're ready
      // to commit, since the invalidation will be merged with the commit.
      host_impl->BlockImplSideInvalidationRequestsForTesting(false);
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (++num_of_commits_) {
      case 1: {
        // The first commit has happened. Run all tasks on the image worker to
        // ensure that the decode completion triggers an impl-side invalidation
        // request.
        FlushImageDecodeTasks();

        // Block notifying the scheduler of this request until we get a commit.
        host_impl->BlockImplSideInvalidationRequestsForTesting(true);
        host_impl->SetNeedsCommit();
      } break;
      case 2: {
        // Ensure that the expected tiles are invalidated on the sync tree.
        PictureLayerImpl* sync_layer_impl = static_cast<PictureLayerImpl*>(
            host_impl->sync_tree()->root_layer_for_testing());
        PictureLayerTiling* sync_tiling =
            sync_layer_impl->picture_layer_tiling_set()
                ->FindTilingWithResolution(TileResolution::HIGH_RESOLUTION);

        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 2; j++) {
            Tile* tile =
                sync_tiling->TileAt(i, j) ? sync_tiling->TileAt(i, j) : nullptr;

            // If this is the pending tree, then only the invalidated tiles
            // exist and have a raster task. If its the active tree, then only
            // the invalidated tiles have a raster task.
            if (i < 3) {
              EXPECT_TRUE(tile->HasRasterTask());
            } else if (host_impl->pending_tree()) {
              EXPECT_EQ(tile, nullptr);
            } else {
              EXPECT_FALSE(tile->HasRasterTask());
            }
          }
        }
        EndTest();
      } break;
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override { EXPECT_EQ(num_of_commits_, 2); }

  int num_of_commits_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostCheckerImagingTestMergeWithMainFrame);

class LayerTreeHostCheckerImagingTestImplSideTree
    : public LayerTreeHostCheckerImagingTest {
  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ++num_of_impl_side_invalidations_;

    // The source_frame_number of the sync tree should be from the first main
    // frame, since this is an impl-side sync tree.
    EXPECT_EQ(host_impl->sync_tree()->source_frame_number(), 0);

    // Ensure that the expected tiles are invalidated on the sync tree.
    PictureLayerImpl* sync_layer_impl = static_cast<PictureLayerImpl*>(
        host_impl->sync_tree()->root_layer_for_testing());
    PictureLayerTiling* sync_tiling =
        sync_layer_impl->picture_layer_tiling_set()->FindTilingWithResolution(
            TileResolution::HIGH_RESOLUTION);

    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 2; j++) {
        Tile* tile =
            sync_tiling->TileAt(i, j) ? sync_tiling->TileAt(i, j) : nullptr;

        // If this is the pending tree, then only the invalidated tiles
        // exist and have a raster task. If its the active tree, then only
        // the invalidated tiles have a raster task.
        if (i < 2) {
          EXPECT_TRUE(tile->HasRasterTask());
        } else if (host_impl->pending_tree()) {
          EXPECT_EQ(tile, nullptr);
        } else {
          EXPECT_FALSE(tile->HasRasterTask());
        }
      }
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    num_of_commits_++;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    num_of_activations_++;
    if (num_of_activations_ == 2)
      EndTest();
  }

  void AfterTest() override {
    EXPECT_EQ(num_of_activations_, 2);
    EXPECT_EQ(num_of_commits_, 1);
    EXPECT_EQ(num_of_impl_side_invalidations_, 1);
  }

  int num_of_activations_ = 0;
  int num_of_commits_ = 0;
  int num_of_impl_side_invalidations_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCheckerImagingTestImplSideTree);

}  // namespace
}  // namespace cc
