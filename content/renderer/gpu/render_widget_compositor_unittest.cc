// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/failure_output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/render_widget.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/fake_renderer_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

using testing::AllOf;
using testing::Field;

namespace content {
namespace {

class MockWebWidget : public blink::WebWidget {
 public:
  MOCK_METHOD1(beginFrame, void(double lastFrameTimeMonotonic));
};

class TestRenderWidget : public RenderWidget {
 public:
  explicit TestRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(compositor_deps,
                     blink::WebPopupTypeNone,
                     blink::WebScreenInfo(),
                     true,
                     false,
                     false) {
    webwidget_ = &mock_webwidget_;
    SetRoutingID(++next_routing_id_);
  }

  MockWebWidget mock_webwidget_;

 protected:
  ~TestRenderWidget() override { webwidget_ = NULL; }
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderWidget);
};

int TestRenderWidget::next_routing_id_ = 0;

class RenderWidgetCompositorTest : public testing::Test {
 public:
  RenderWidgetCompositorTest()
      : compositor_deps_(new FakeCompositorDependencies),
        render_widget_(new TestRenderWidget(compositor_deps_.get())),
        render_widget_compositor_(
            RenderWidgetCompositor::Create(render_widget_.get(),
                                           compositor_deps_.get())) {}
  ~RenderWidgetCompositorTest() override {}

 protected:
  base::MessageLoop loop_;
  MockRenderThread render_thread_;
  scoped_ptr<FakeCompositorDependencies> compositor_deps_;
  scoped_refptr<TestRenderWidget> render_widget_;
  scoped_ptr<RenderWidgetCompositor> render_widget_compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorTest);
};

TEST_F(RenderWidgetCompositorTest, BeginMainFrame) {
  base::TimeTicks frame_time(base::TimeTicks() +
                             base::TimeDelta::FromSeconds(1));
  base::TimeTicks deadline(base::TimeTicks() + base::TimeDelta::FromSeconds(2));
  base::TimeDelta interval(base::TimeDelta::FromSeconds(3));
  cc::BeginFrameArgs args(
      cc::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time, deadline,
                                 interval, cc::BeginFrameArgs::NORMAL));

  EXPECT_CALL(render_widget_->mock_webwidget_, beginFrame(1));

  render_widget_compositor_->BeginMainFrame(args);
}

class RenderWidgetCompositorOutputSurface;

class RenderWidgetOutputSurface : public TestRenderWidget {
 public:
  explicit RenderWidgetOutputSurface(CompositorDependencies* compositor_deps)
      : TestRenderWidget(compositor_deps), compositor_(NULL) {}
  void SetCompositor(RenderWidgetCompositorOutputSurface* compositor);

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback) override;

 protected:
  ~RenderWidgetOutputSurface() override {}

 private:
  RenderWidgetCompositorOutputSurface* compositor_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetOutputSurface);
};

// Verify that failing to create an output surface will cause the compositor
// to attempt to repeatedly create another output surface.  After enough
// failures, verify that it attempts to create a fallback output surface.
// The use null output surface parameter allows testing whether failures
// from RenderWidget (couldn't create an output surface) vs failures from
// the compositor (couldn't bind the output surface) are handled identically.
class RenderWidgetCompositorOutputSurface : public RenderWidgetCompositor {
 public:
  RenderWidgetCompositorOutputSurface(RenderWidget* widget,
                                      CompositorDependencies* compositor_deps)
      : RenderWidgetCompositor(widget, compositor_deps),
        num_failures_before_success_(0),
        expected_successes_(0),
        expected_fallback_successes_(0),
        expected_requests_(0),
        num_requests_(0),
        num_requests_since_last_success_(0),
        num_successes_(0),
        num_fallback_successes_(0),
        num_failures_(0),
        last_create_was_fallback_(false),
        use_null_output_surface_(true) {}

  using RenderWidgetCompositor::Initialize;

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback) {
    EXPECT_EQ(num_requests_since_last_success_ >
                  OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
              fallback);
    last_create_was_fallback_ = fallback;
    bool success = num_failures_ >= num_failures_before_success_;
    if (success) {
      scoped_ptr<cc::TestWebGraphicsContext3D> context =
          cc::TestWebGraphicsContext3D::Create();
      // Image support required for synchronous compositing.
      context->set_support_image(true);
      // Create delegating surface so that max_pending_frames = 1.
      return cc::FakeOutputSurface::CreateDelegating3d(std::move(context));
    }
    return use_null_output_surface_
               ? nullptr
               : make_scoped_ptr(new cc::FailureOutputSurface(true));
  }

  // Force a new output surface to be created.
  void SynchronousComposite() {
   layer_tree_host()->DidLoseOutputSurface();

   base::TimeTicks some_time;
   layer_tree_host()->Composite(some_time);
  }

  void RequestNewOutputSurface() override {
    ++num_requests_;
    ++num_requests_since_last_success_;
    RenderWidgetCompositor::RequestNewOutputSurface();
  }

  void DidInitializeOutputSurface() override {
    if (last_create_was_fallback_)
      ++num_fallback_successes_;
    else
      ++num_successes_;

    if (num_requests_ == expected_requests_) {
      EndTest();
    } else {
      num_requests_since_last_success_ = 0;
      RenderWidgetCompositor::DidInitializeOutputSurface();
      // Post the synchronous composite task so that it is not called
      // reentrantly as a part of RequestNewOutputSurface.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&RenderWidgetCompositorOutputSurface::SynchronousComposite,
                     base::Unretained(this)));
    }
  }

  void DidFailToInitializeOutputSurface() override {
    ++num_failures_;
    if (num_requests_ == expected_requests_) {
      EndTest();
      return;
    }

    RenderWidgetCompositor::DidFailToInitializeOutputSurface();
  }

  void SetUp(bool use_null_output_surface,
             int num_failures_before_success,
             int expected_successes,
             int expected_fallback_succeses) {
    use_null_output_surface_ = use_null_output_surface;
    num_failures_before_success_ = num_failures_before_success;
    expected_successes_ = expected_successes;
    expected_fallback_successes_ = expected_fallback_succeses;
    expected_requests_ = num_failures_before_success_ + expected_successes_ +
                         expected_fallback_successes_;
  }

  void EndTest() { base::MessageLoop::current()->QuitWhenIdle(); }

  void AfterTest() {
    EXPECT_EQ(num_failures_before_success_, num_failures_);
    EXPECT_EQ(expected_successes_, num_successes_);
    EXPECT_EQ(expected_fallback_successes_, num_fallback_successes_);
    EXPECT_EQ(expected_requests_, num_requests_);
  }

 private:
  int num_failures_before_success_;
  int expected_successes_;
  int expected_fallback_successes_;
  int expected_requests_;
  int num_requests_;
  int num_requests_since_last_success_;
  int num_successes_;
  int num_fallback_successes_;
  int num_failures_;
  bool last_create_was_fallback_;
  bool use_null_output_surface_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorOutputSurface);
};

class RenderWidgetCompositorOutputSurfaceTest : public testing::Test {
 public:
  RenderWidgetCompositorOutputSurfaceTest()
      : compositor_deps_(new FakeCompositorDependencies),
        render_widget_(new RenderWidgetOutputSurface(compositor_deps_.get())) {
    render_widget_compositor_.reset(new RenderWidgetCompositorOutputSurface(
        render_widget_.get(), compositor_deps_.get()));
    render_widget_compositor_->Initialize();
    render_widget_->SetCompositor(render_widget_compositor_.get());
  }

  void RunTest(bool use_null_output_surface,
               int num_failures_before_success,
               int expected_successes,
               int expected_fallback_succeses) {
    render_widget_compositor_->SetUp(
        use_null_output_surface, num_failures_before_success,
        expected_successes, expected_fallback_succeses);
    render_widget_compositor_->setVisible(true);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RenderWidgetCompositorOutputSurface::SynchronousComposite,
                   base::Unretained(render_widget_compositor_.get())));
    base::MessageLoop::current()->Run();
    render_widget_compositor_->AfterTest();
  }

 protected:
  base::MessageLoop ye_olde_message_loope_;
  MockRenderThread render_thread_;
  scoped_ptr<FakeCompositorDependencies> compositor_deps_;
  scoped_refptr<RenderWidgetOutputSurface> render_widget_;
  scoped_ptr<RenderWidgetCompositorOutputSurface> render_widget_compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorOutputSurfaceTest);
};

scoped_ptr<cc::OutputSurface> RenderWidgetOutputSurface::CreateOutputSurface(
    bool fallback) {
  return compositor_->CreateOutputSurface(fallback);
}

void RenderWidgetOutputSurface::SetCompositor(
    RenderWidgetCompositorOutputSurface* compositor) {
  compositor_ = compositor;
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, SucceedOnce) {
  RunTest(false, 0, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, SucceedTwice) {
  RunTest(false, 0, 2, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FailOnceNull) {
  static_assert(
      RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK >= 2,
      "Adjust the values of this test if this fails");
  RunTest(true, 1, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FailOnceBind) {
  static_assert(
      RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK >= 2,
      "Adjust the values of this test if this fails");
  RunTest(false, 1, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessNull) {
  RunTest(true, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          0, 1);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessBind) {
  RunTest(false, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          0, 1);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessNormalSuccess) {
  // The first success is a fallback, but the next should not be a fallback.
  RunTest(false, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          1, 1);
}

}  // namespace
}  // namespace content
