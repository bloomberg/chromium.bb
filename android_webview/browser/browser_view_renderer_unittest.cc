// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>
#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/compositor_frame_consumer.h"
#include "android_webview/browser/test/rendering_test.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

class SmokeTest : public RenderingTest {
  void StartTest() override { browser_view_renderer_->PostInvalidate(); }

  void DidDrawOnRT(RenderThreadManager* functor) override {
    EndTest();
  }
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

  void DidDrawOnRT(RenderThreadManager* functor) override {
    EndTest();
  }
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

  bool WillDrawOnRT(RenderThreadManager* functor,
                    AwDrawGLInfo* draw_info) override {
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

  void DidDrawOnRT(RenderThreadManager* functor) override {
    draw_gl_count_on_rt_++;
  }

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

class SwitchOutputSurfaceIdTest : public RenderingTest {
 public:
  struct FrameInfo {
    uint32_t output_surface_id;
    cc::ResourceId resource_id;  // Each frame contains a single resource.
  };

  void StartTest() override {
    last_output_surface_id_ = 0;
    FrameInfo infos[] = {
        // First output surface.
        {0u, 1u}, {0u, 1u}, {0u, 2u}, {0u, 2u}, {0u, 3u}, {0u, 3u}, {0u, 4u},
        // Second output surface.
        {1u, 1u}, {1u, 1u}, {1u, 2u}, {1u, 2u}, {1u, 3u}, {1u, 3u}, {1u, 4u},
    };
    for (const auto& info : infos) {
      content::SynchronousCompositor::Frame frame;
      frame.output_surface_id = info.output_surface_id;
      frame.frame = ConstructEmptyFrame();
      cc::TransferableResource resource;
      resource.id = info.resource_id;
      frame.frame->delegated_frame_data->resource_list.push_back(resource);
      frames_.push(std::move(frame));

      // Keep a id -> count map for the last ouptut_surface_id.
      if (last_output_surface_id_ != info.output_surface_id) {
        expected_return_count_.clear();
        last_output_surface_id_ = info.output_surface_id;
      }
      if (expected_return_count_.count(info.resource_id)) {
        expected_return_count_[info.resource_id]++;
      } else {
        expected_return_count_[info.resource_id] = 1;
      }
    }

    browser_view_renderer_->PostInvalidate();
  }

  void WillOnDraw() override {
    if (!frames_.empty()) {
      compositor_->SetHardwareFrame(frames_.front().output_surface_id,
                                    std::move(frames_.front().frame));
    }
  }

  void DidOnDraw(bool success) override {
    EXPECT_TRUE(success);
    if (frames_.empty()) {
      ui_task_runner_->PostTask(
          FROM_HERE, base::Bind(&SwitchOutputSurfaceIdTest::CheckResults,
                                base::Unretained(this)));
    } else {
      frames_.pop();
      browser_view_renderer_->PostInvalidate();
    }
  }

  void CheckResults() {
    window_->Detach();
    window_.reset();

    // Make sure resources for the last output surface are returned.
    content::TestSynchronousCompositor::FrameAckArray returned_resources_array;
    compositor_->SwapReturnedResources(&returned_resources_array);
    for (const auto& resources : returned_resources_array) {
      if (resources.output_surface_id != last_output_surface_id_)
        continue;
      for (const auto& returned_resource : resources.resources) {
        EXPECT_TRUE(!!expected_return_count_.count(returned_resource.id));
        EXPECT_GE(expected_return_count_[returned_resource.id],
                  returned_resource.count);
        expected_return_count_[returned_resource.id] -=
            returned_resource.count;
        if (!expected_return_count_[returned_resource.id])
          expected_return_count_.erase(returned_resource.id);
      }
    }
    EXPECT_TRUE(expected_return_count_.empty());

    EndTest();
  }

 private:
  std::queue<content::SynchronousCompositor::Frame> frames_;
  uint32_t last_output_surface_id_;
  std::map<cc::ResourceId, int> expected_return_count_;
};

RENDERING_TEST_F(SwitchOutputSurfaceIdTest);

}  // namespace android_webview
