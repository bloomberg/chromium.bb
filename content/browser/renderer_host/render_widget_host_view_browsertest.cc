// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
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

#if defined(OS_MACOSX)
  void SetupCompositingSurface() {
    NavigateToURL(shell(), net::FilePathToFileURL(
        test_dir_.AppendASCII("rwhv_compositing_static.html")));

    RenderViewHost* const rwh =
        shell()->web_contents()->GetRenderViewHost();
    RenderWidgetHostViewPort* rwhvp =
        static_cast<RenderWidgetHostViewPort*>(rwh->GetView());

    // Wait until an IoSurface is created by repeatedly resizing the window.
    // TODO(justinlin): Find a better way to force an IoSurface when possible.
    int increment = 0;
    while (!rwhvp->HasAcceleratedSurface(gfx::Size())) {
      base::RunLoop run_loop;
      SetWindowBounds(shell()->window(), gfx::Rect(size_.width() + increment,
                                                   size_.height()));
      // Wait for any ViewHostMsg_CompositorSurfaceBuffersSwapped message.
      run_loop.RunUntilIdle();
      increment++;
      ASSERT_LT(increment, 50);
    }
  }
#endif

  void FinishCopyFromBackingStore(bool expected_result,
                                  const base::Callback<void ()> quit_closure,
                                  bool result,
                                  const SkBitmap& bitmap) {
    quit_closure.Run();
    ASSERT_EQ(expected_result, result);
    if (expected_result)
      ASSERT_FALSE(bitmap.empty());
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

#if defined(OS_MACOSX)
// Tests that the callback passed to CopyFromBackingStore is always called, even
// when the RenderWidgetHost is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       MacAsyncCopyFromBackingStoreCallbackTest) {
  if (!IOSurfaceSupport::Initialize())
    return;

  SetupCompositingSurface();

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

  ASSERT_TRUE(finish_called_);
}

// TODO(justinlin): Enable this test for other platforms.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       MacAsyncCopyFromBackingStoreTest) {
  if (!IOSurfaceSupport::Initialize())
    return;

  SetupCompositingSurface();

  base::RunLoop run_loop;
  shell()->web_contents()->GetRenderViewHost()->CopyFromBackingStore(
      gfx::Rect(),
      size_,
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), true, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(finish_called_);
}

static void DeliverFrameFunc(base::Closure quit_closure,
                             bool* frame_captured_out,
                             base::Time timestamp,
                             bool frame_captured) {
  *frame_captured_out = frame_captured;
  quit_closure.Run();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       MacFrameSubscriberTest) {
  if (!IOSurfaceSupport::Initialize())
    return;

  SetupCompositingSurface();

  base::RunLoop run_loop;
  RenderWidgetHostViewPort* view = RenderWidgetHostViewPort::FromRWHV(
      shell()->web_contents()->GetRenderViewHost()->GetView());
  ASSERT_TRUE(view);

  EXPECT_TRUE(view->CanSubscribeFrame());

  bool frame_captured = false;
  view->BeginFrameSubscription(
      new FakeFrameSubscriber(base::Bind(&DeliverFrameFunc,
                                         run_loop.QuitClosure(),
                                         &frame_captured)));

  // Do a resize of the window to trigger a repaint and present.
  SetWindowBounds(shell()->window(), gfx::Rect(size_));
  run_loop.Run();
  view->EndFrameSubscription();

  EXPECT_TRUE(frame_captured);
}

#endif

}  // namespace content
