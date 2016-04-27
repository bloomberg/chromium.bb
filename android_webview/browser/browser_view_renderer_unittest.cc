// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>
#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/compositor_frame_consumer.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/browser/test/rendering_test.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

class SmokeTest : public RenderingTest {
  void StartTest() override { browser_view_renderer_->PostInvalidate(); }

  void DidDrawOnRT() override { EndTest(); }
};

RENDERING_TEST_F(SmokeTest);

class ClearViewTest : public RenderingTest {
 public:
  ClearViewTest() : on_draw_count_(0) {}

  void StartTest() override {
    browser_view_renderer_->PostInvalidate();
    browser_view_renderer_->ClearView();
  }

  void DidOnDraw(bool success) override {
    on_draw_count_++;
    if (on_draw_count_ == 1) {
      // First OnDraw should be skipped due to ClearView.
      EXPECT_FALSE(success);
      browser_view_renderer_->DidUpdateContent();  // Unset ClearView.
      browser_view_renderer_->PostInvalidate();
    } else {
      // Following OnDraws should succeed.
      EXPECT_TRUE(success);
    }
  }

  void DidDrawOnRT() override { EndTest(); }

 private:
  int on_draw_count_;
};

RENDERING_TEST_F(ClearViewTest);

class TestAnimateInAndOutOfScreen : public RenderingTest {
 public:
  TestAnimateInAndOutOfScreen() : on_draw_count_(0), draw_gl_count_on_rt_(0) {}

  void StartTest() override {
    new_constraints_ = ParentCompositorDrawConstraints(
        false, gfx::Transform(), window_->surface_size().IsEmpty());
    new_constraints_.transform.Scale(2.0, 2.0);
    browser_view_renderer_->PostInvalidate();
  }

  void WillOnDraw() override {
    RenderingTest::WillOnDraw();
    // Step 0: A single onDraw on screen. The parent draw constraints
    // of the BVR will updated to be the initial constraints.
    // Step 1: A single onDrraw off screen. The parent draw constraints of the
    // BVR will be updated to the new constraints.
    // Step 2: This onDraw is to introduce the DrawGL that animates the
    // webview onto the screen on render thread. End the test when the parent
    // draw constraints of BVR is updated to initial constraints.
    if (on_draw_count_ == 1 || on_draw_count_ == 2)
      browser_view_renderer_->PrepareToDraw(gfx::Vector2d(), gfx::Rect());
  }

  void DidOnDraw(bool success) override {
    EXPECT_TRUE(success);
    on_draw_count_++;
  }

  bool WillDrawOnRT(AwDrawGLInfo* draw_info) override {
    if (draw_gl_count_on_rt_ == 1) {
      draw_gl_count_on_rt_++;
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&RenderingTest::PostInvalidate, base::Unretained(this)));
      return false;
    }

    draw_info->width = window_->surface_size().width();
    draw_info->height = window_->surface_size().height();
    draw_info->is_layer = false;

    gfx::Transform transform;
    if (draw_gl_count_on_rt_ == 0)
      transform = new_constraints_.transform;

    transform.matrix().asColMajorf(draw_info->transform);
    return true;
  }

  void DidDrawOnRT() override { draw_gl_count_on_rt_++; }

  bool DrawConstraintsEquals(
      const ParentCompositorDrawConstraints& constraints1,
      const ParentCompositorDrawConstraints& constraints2) {
    if (constraints1.is_layer != constraints2.is_layer ||
        constraints1.transform != constraints2.transform)
      return false;

    return !constraints1.is_layer ||
           constraints1.surface_rect_empty == constraints2.surface_rect_empty;
  }

  void OnParentDrawConstraintsUpdated() override {
    ParentCompositorDrawConstraints constraints =
        GetCompositorFrameConsumer()->GetParentDrawConstraintsOnUI();
    switch (on_draw_count_) {
      case 1u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, new_constraints_));
        break;
      case 3u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, initial_constraints_));
        EndTest();
        break;
      // There will be a following 4th onDraw. But the hardware renderer won't
      // post back the draw constraints in DrawGL because the constraints
      // don't change.
      default:
        FAIL();
    }
  }

 private:
  int on_draw_count_;
  int draw_gl_count_on_rt_;
  ParentCompositorDrawConstraints initial_constraints_;
  ParentCompositorDrawConstraints new_constraints_;
};

RENDERING_TEST_F(TestAnimateInAndOutOfScreen);

class CompositorNoFrameTest : public RenderingTest {
 public:
  CompositorNoFrameTest() : on_draw_count_(0) {}

  void StartTest() override {
    browser_view_renderer_->PostInvalidate();
  }

  void WillOnDraw() override {
    if (0 == on_draw_count_) {
      // No frame from compositor.
    } else if (1 == on_draw_count_) {
      compositor_->SetHardwareFrame(0u, ConstructEmptyFrame());
    } else if (2 == on_draw_count_) {
      // No frame from compositor.
    }
    // There may be trailing invalidates.
  }

  void DidOnDraw(bool success) override {
    if (0 == on_draw_count_) {
      // Should fail as there has been no frames from compositor.
      EXPECT_FALSE(success);
      browser_view_renderer_->PostInvalidate();
    } else if (1 == on_draw_count_) {
      // Should succeed with frame from compositor.
      EXPECT_TRUE(success);
      browser_view_renderer_->PostInvalidate();
    } else if (2 == on_draw_count_) {
      // Should still succeed with last frame, even if no frame from compositor.
      EXPECT_TRUE(success);
      EndTest();
    }
    on_draw_count_++;
  }

 private:
  int on_draw_count_;
};

RENDERING_TEST_F(CompositorNoFrameTest);

class ResourceRenderingTest : public RenderingTest {
 public:
  using ResourceCountMap = std::map<cc::ResourceId, int>;
  using OutputSurfaceResourceCountMap = std::map<uint32_t, ResourceCountMap>;

  virtual std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) = 0;

  void StartTest() override {
    frame_number_ = 0;
    AdvanceFrame();
  }

  void WillOnDraw() override {
    if (next_frame_) {
      compositor_->SetHardwareFrame(next_frame_->output_surface_id,
                                    std::move(next_frame_->frame));
    }
  }

  void DidOnDraw(bool success) override {
    EXPECT_EQ(next_frame_ != nullptr, success);
    if (!AdvanceFrame()) {
      ui_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ResourceRenderingTest::CheckResults,
                                           base::Unretained(this)));
    }
  }

  OutputSurfaceResourceCountMap GetReturnedResourceCounts() {
    OutputSurfaceResourceCountMap counts;
    content::TestSynchronousCompositor::FrameAckArray returned_resources_array;
    compositor_->SwapReturnedResources(&returned_resources_array);
    for (const auto& resources : returned_resources_array) {
      for (const auto& returned_resource : resources.resources) {
        counts[resources.output_surface_id][returned_resource.id] +=
            returned_resource.count;
      }
    }
    return counts;
  }

  virtual void CheckResults() = 0;

 private:
  bool AdvanceFrame() {
    next_frame_ = GetFrame(frame_number_++);
    if (next_frame_) {
      browser_view_renderer_->PostInvalidate();
      return true;
    }
    return false;
  }

  std::unique_ptr<content::SynchronousCompositor::Frame> next_frame_;
  int frame_number_;
};

class SwitchOutputSurfaceIdTest : public ResourceRenderingTest {
  struct FrameInfo {
    uint32_t output_surface_id;
    cc::ResourceId resource_id;  // Each frame contains a single resource.
  };

  std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) override {
    static const FrameInfo infos[] = {
        // First output surface.
        {0u, 1u},
        {0u, 1u},
        {0u, 2u},
        {0u, 2u},
        {0u, 3u},
        {0u, 3u},
        {0u, 4u},
        // Second output surface.
        {1u, 1u},
        {1u, 1u},
        {1u, 2u},
        {1u, 2u},
        {1u, 3u},
        {1u, 3u},
        {1u, 4u},
    };
    if (frame_number >= static_cast<int>(arraysize(infos))) {
      return nullptr;
    }

    std::unique_ptr<content::SynchronousCompositor::Frame> frame(
        new content::SynchronousCompositor::Frame);
    frame->output_surface_id = infos[frame_number].output_surface_id;
    frame->frame = ConstructFrame(infos[frame_number].resource_id);

    if (last_output_surface_id_ != infos[frame_number].output_surface_id) {
      expected_return_count_.clear();
      last_output_surface_id_ = infos[frame_number].output_surface_id;
    }
    ++expected_return_count_[infos[frame_number].resource_id];
    return frame;
  }

  void StartTest() override {
    last_output_surface_id_ = -1U;
    ResourceRenderingTest::StartTest();
  }

  void CheckResults() override {
    GetCompositorFrameConsumer()->DeleteHardwareRendererOnUI();
    window_->Detach();
    window_.reset();

    // Make sure resources for the last output surface are returned.
    EXPECT_EQ(expected_return_count_,
              GetReturnedResourceCounts()[last_output_surface_id_]);
    EndTest();
  }

 private:
  uint32_t last_output_surface_id_;
  ResourceCountMap expected_return_count_;
};

RENDERING_TEST_F(SwitchOutputSurfaceIdTest);

class RenderThreadManagerDeletionTest : public ResourceRenderingTest {
  std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) override {
    if (frame_number > 0) {
      return nullptr;
    }

    const uint32_t output_surface_id = 0u;
    const cc::ResourceId resource_id =
        static_cast<cc::ResourceId>(frame_number);

    std::unique_ptr<content::SynchronousCompositor::Frame> frame(
        new content::SynchronousCompositor::Frame);
    frame->output_surface_id = output_surface_id;
    frame->frame = ConstructFrame(resource_id);
    ++expected_return_count_[output_surface_id][resource_id];
    return frame;
  }

  void CheckResults() override {
    OutputSurfaceResourceCountMap resource_counts;
    render_thread_manager_.reset();
    // Make sure resources for the last frame are returned.
    EXPECT_EQ(expected_return_count_, GetReturnedResourceCounts());
    EndTest();
  }

 private:
  OutputSurfaceResourceCountMap expected_return_count_;
};

RENDERING_TEST_F(RenderThreadManagerDeletionTest);

}  // namespace android_webview
