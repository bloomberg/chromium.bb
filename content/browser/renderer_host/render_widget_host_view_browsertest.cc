// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "media/base/video_frame.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/compositor_setup.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

namespace content {

class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest() : finish_called_(false), size_(400, 300) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ui::DisableTestCompositor();
  }

  bool CheckAcceleratedCompositingActive() {
    RenderWidgetHostImpl* impl =
        RenderWidgetHostImpl::From(
            shell()->web_contents()->GetRenderWidgetHostView()->
                GetRenderWidgetHost());
    return impl->is_accelerated_compositing_active();
  }

  bool CheckCompositingSurface() {
#if defined(OS_WIN)
    if (!GpuDataManagerImpl::GetInstance()->IsUsingAcceleratedSurface())
      return false;
#endif

    RenderViewHost* const rwh =
        shell()->web_contents()->GetRenderViewHost();
    RenderWidgetHostViewPort* rwhvp =
        static_cast<RenderWidgetHostViewPort*>(rwh->GetView());
    bool ret = !rwhvp->GetCompositingSurface().is_null();
#if defined(OS_MACOSX)
    ret &= rwhvp->HasAcceleratedSurface(gfx::Size());
#endif
    return ret;
  }

  bool SetupCompositingSurface() {
#if defined(OS_MACOSX)
    if (!IOSurfaceSupport::Initialize())
      return false;
#endif
    NavigateToURL(shell(), net::FilePathToFileURL(
        test_dir_.AppendASCII("rwhv_compositing_animation.html")));
    if (!CheckAcceleratedCompositingActive())
      return false;

    // The page is now accelerated composited but a compositing surface might
    // not be available immediately so wait for it.
    while (!CheckCompositingSurface()) {
      base::RunLoop run_loop;
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(10));
      run_loop.Run();
    }
    return true;
  }

  bool SetupNonCompositing() {
    NavigateToURL(shell(), net::FilePathToFileURL(
        test_dir_.AppendASCII("rwhv_compositing_static.html")));
    return !CheckCompositingSurface();
  }

  void FinishCopyFromBackingStore(bool expected_result,
                                  const base::Closure& quit_closure,
                                  bool result,
                                  const SkBitmap& bitmap) {
    quit_closure.Run();
    EXPECT_EQ(expected_result, result);
    if (expected_result)
      EXPECT_FALSE(bitmap.empty());
    finish_called_ = true;
  }

  void FinishCopyFromCompositingSurface(bool expected_result,
                                        const base::Closure& quit_closure,
                                        bool result) {
    if (!quit_closure.is_null())
      quit_closure.Run();
    EXPECT_EQ(expected_result, result);
    finish_called_ = true;
  }

 protected:
  base::FilePath test_dir_;
  bool finish_called_;
  gfx::Size size_;
};

class FakeFrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FakeFrameSubscriber(
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback)
      : callback_(callback) {
  }

  virtual bool ShouldCaptureFrame(
      scoped_refptr<media::VideoFrame>* storage,
      DeliverFrameCallback* callback) OVERRIDE {
    *storage = media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
    *callback = callback_;
    return true;
  }

 private:
  DeliverFrameCallback callback_;
};

#if defined(OS_MACOSX) || defined(OS_WIN)

static void DeliverFrameFunc(const scoped_refptr<base::MessageLoopProxy>& loop,
                             base::Closure quit_closure,
                             bool* frame_captured_out,
                             base::Time timestamp,
                             bool frame_captured) {
  *frame_captured_out = frame_captured;
  if (!quit_closure.is_null())
    loop->PostTask(FROM_HERE, quit_closure);
}

#endif

#if defined(OS_MACOSX)
// Tests that the callback passed to CopyFromBackingStore is always called, even
// when the RenderWidgetHost is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       MacAsyncCopyFromBackingStoreCallbackTest) {
  if (!SetupCompositingSurface()) {
    LOG(WARNING) << "Accelerated compositing not running.";
    return;
  }

  base::RunLoop run_loop;
  RenderViewHost* const rwh =
      shell()->web_contents()->GetRenderViewHost();
  RenderWidgetHostViewPort* rwhvp =
      static_cast<RenderWidgetHostViewPort*>(rwh->GetView());

  rwh->CopyFromBackingStore(
      gfx::Rect(),
      size_,
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), false, run_loop.QuitClosure()));

  // Delete the surface before the callback is run. This is synchronous until
  // we get to the copy_timer_, so we will always end up in the destructor
  // before the timer fires.
  rwhvp->AcceleratedSurfaceRelease();
  run_loop.Run();

  EXPECT_TRUE(finish_called_);
}

// Tests that the callback passed to CopyFromCompositingSurfaceToVideoFrame is
// always called, even when the RenderWidgetHost is deleting in the middle of
// an async copy.
IN_PROC_BROWSER_TEST_F(
    RenderWidgetHostViewBrowserTest,
    MacAsyncCopyFromCompositingSurfaceToVideoFrameCallbackTest) {
  if (!SetupCompositingSurface()) {
    LOG(WARNING) << "Accelerated compositing not running.";
    return;
  }

  base::RunLoop run_loop;
  RenderViewHost* const rwh =
      shell()->web_contents()->GetRenderViewHost();
  RenderWidgetHostViewPort* rwhvp =
      static_cast<RenderWidgetHostViewPort*>(rwh->GetView());

  scoped_refptr<media::VideoFrame> dest =
      media::VideoFrame::CreateBlackFrame(size_);
  rwhvp->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(rwhvp->GetViewBounds().size()), dest, base::Bind(
          &RenderWidgetHostViewBrowserTest::FinishCopyFromCompositingSurface,
          base::Unretained(this), false, run_loop.QuitClosure()));

  // Delete the surface before the callback is run. This is synchronous until
  // we get to the copy_timer_, so we will always end up in the destructor
  // before the timer fires.
  rwhvp->AcceleratedSurfaceRelease();
  run_loop.Run();

  ASSERT_TRUE(finish_called_);
}
#endif

#if (defined(OS_WIN) && !defined(USE_AURA)) || defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       FrameSubscriberTest) {
  if (!SetupCompositingSurface()) {
    LOG(WARNING) << "Accelerated compositing not running.";
    return;
  }

  base::RunLoop run_loop;
  RenderWidgetHostViewPort* view = RenderWidgetHostViewPort::FromRWHV(
      shell()->web_contents()->GetRenderViewHost()->GetView());
  ASSERT_TRUE(view);

  if (!view->CanSubscribeFrame()) {
    LOG(WARNING) << "Frame subscription no supported on this platform.";
    return;
  }

  bool frame_captured = false;
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
      new FakeFrameSubscriber(base::Bind(&DeliverFrameFunc,
                                         base::MessageLoopProxy::current(),
                                         run_loop.QuitClosure(),
                                         &frame_captured)));
  view->BeginFrameSubscription(subscriber.Pass());
  run_loop.Run();
  view->EndFrameSubscription();
  EXPECT_TRUE(frame_captured);
}

// Test copying from backing store when page is non-accelerated-composited.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore) {
  SetupNonCompositing();
  base::RunLoop run_loop;

  shell()->web_contents()->GetRenderViewHost()->CopyFromBackingStore(
      gfx::Rect(),
      size_,
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), true, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(finish_called_);
}
#endif

#if defined(OS_MACOSX)
// Test that we can copy twice from an accelerated composited page.
// This test is only running on Mac because this is the only platform that
// we can reliably detect that accelerated surface is in use.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest, CopyTwice) {
  if (!SetupCompositingSurface()) {
    LOG(WARNING) << "Accelerated compositing not running.";
    return;
  }

  base::RunLoop run_loop;
  RenderViewHost* const rwh =
      shell()->web_contents()->GetRenderViewHost();
  RenderWidgetHostViewPort* rwhvp =
      static_cast<RenderWidgetHostViewPort*>(rwh->GetView());
  scoped_refptr<media::VideoFrame> dest =
      media::VideoFrame::CreateBlackFrame(size_);

  bool first_frame_captured = false;
  bool second_frame_captured = false;
  rwhvp->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(rwhvp->GetViewBounds().size()), dest,
      base::Bind(&DeliverFrameFunc,
                 base::MessageLoopProxy::current(),
                 base::Closure(),
                 &first_frame_captured,
                 base::Time::Now()));
  rwhvp->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(rwhvp->GetViewBounds().size()), dest,
      base::Bind(&DeliverFrameFunc,
                 base::MessageLoopProxy::current(),
                 run_loop.QuitClosure(),
                 &second_frame_captured,
                 base::Time::Now()));
  run_loop.Run();

  EXPECT_TRUE(first_frame_captured);
  EXPECT_TRUE(second_frame_captured);
}
#endif

}  // namespace content
