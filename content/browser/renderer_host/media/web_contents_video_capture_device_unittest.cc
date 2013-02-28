// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"

#include "base/bind_helpers.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
namespace {
const int kTestWidth = 1280;
const int kTestHeight = 720;
const int kBytesPerPixel = 4;
const int kTestFramesPerSecond = 20;
const base::TimeDelta kWaitTimeout = base::TimeDelta::FromMilliseconds(2000);
const SkColor kNothingYet = 0xdeadbeef;
const SkColor kNotInterested = ~kNothingYet;

void DeadlineExceeded(base::Closure quit_closure) {
  if (!base::debug::BeingDebugged()) {
    FAIL() << "Deadline exceeded while waiting, quitting";
    quit_closure.Run();
  } else {
    LOG(WARNING) << "Deadline exceeded; test would fail if debugger weren't "
                 << "attached.";
  }
}

void RunCurrentLoopWithDeadline() {
  base::Timer deadline(false, false);
  deadline.Start(FROM_HERE, kWaitTimeout, base::Bind(
      &DeadlineExceeded, MessageLoop::current()->QuitClosure()));
  MessageLoop::current()->Run();
  deadline.Stop();
}

// Thread-safe class that controls the source pattern to be captured by the
// system under test. The lifetime of this class is greater than the lifetime
// of all objects that reference it, so it does not need to be reference
// counted.
class CaptureTestSourceController {
 public:
  CaptureTestSourceController()
      : color_(SK_ColorMAGENTA),
        copy_result_size_(kTestWidth, kTestHeight),
        can_copy_to_video_frame_(false) {}

  void SetSolidColor(SkColor color) {
    base::AutoLock guard(lock_);
    color_ = color;
  }

  SkColor GetSolidColor() {
    base::AutoLock guard(lock_);
    return color_;
  }

  void SetCopyResultSize(int width, int height) {
    base::AutoLock guard(lock_);
    copy_result_size_ = gfx::Size(width, height);
  }

  gfx::Size GetCopyResultSize() {
    base::AutoLock guard(lock_);
    return copy_result_size_;
  }

  void SignalBackingStoreCopy() {
    // TODO(nick): This actually should always be happening on the UI thread.
    base::AutoLock guard(lock_);
    if (!copy_done_.is_null()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, copy_done_);
      copy_done_.Reset();
    }
  }

  void SetCanCopyToVideoFrame(bool value) {
    base::AutoLock guard(lock_);
    can_copy_to_video_frame_ = value;
  }

  bool CanCopyToVideoFrame() {
    base::AutoLock guard(lock_);
    return can_copy_to_video_frame_;
  }

  void WaitForNextBackingStoreCopy() {
    {
      base::AutoLock guard(lock_);
      copy_done_ = MessageLoop::current()->QuitClosure();
    }
    RunCurrentLoopWithDeadline();
  }

  void OnShutdown() {
    base::AutoLock guard(lock_);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, shutdown_hook_);
  }

  void SetShutdownHook(base::Closure shutdown_hook) {
    base::AutoLock guard(lock_);
    shutdown_hook_ = shutdown_hook;
  }

 private:
  base::Lock lock_;  // Guards changes to all members.
  SkColor color_;
  gfx::Size copy_result_size_;
  bool can_copy_to_video_frame_;
  base::Closure copy_done_;
  base::Closure shutdown_hook_;

  DISALLOW_COPY_AND_ASSIGN(CaptureTestSourceController);
};

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromCompositingSurfaceToVideoFrame(), and which allows the video-frame
// readback path to be switched on and off. The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestView : public TestRenderWidgetHostView {
 public:
  explicit CaptureTestView(RenderWidgetHostImpl* rwh,
                           CaptureTestSourceController* controller)
      : TestRenderWidgetHostView(rwh),
        controller_(controller) {}
  virtual ~CaptureTestView() {}

  // TestRenderWidgetHostView overrides.
  virtual gfx::Rect GetViewBounds() const OVERRIDE {
    return gfx::Rect(100, 100, 100 + kTestWidth, 100 + kTestHeight);
  }

  virtual bool CanCopyToVideoFrame() const OVERRIDE {
    return controller_->CanCopyToVideoFrame();
  }

  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE {
    SkColor c = controller_->GetSolidColor();
    media::FillYUV(target, SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
    callback.Run(true);
    controller_->SignalBackingStoreCopy();
  }

 private:
  CaptureTestSourceController* const controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestView);
};

#if defined(COMPILER_MSVC)
// MSVC warns on diamond inheritance. See comment for same warning on
// RenderViewHostImpl.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromBackingStore(). The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestRenderViewHost : public TestRenderViewHost {
 public:
  CaptureTestRenderViewHost(SiteInstance* instance,
                            RenderViewHostDelegate* delegate,
                            RenderWidgetHostDelegate* widget_delegate,
                            int routing_id,
                            bool swapped_out,
                            CaptureTestSourceController* controller)
      : TestRenderViewHost(instance, delegate, widget_delegate, routing_id,
                           swapped_out),
        controller_(controller) {
    // Override the default view installed by TestRenderViewHost; we need
    // our special subclass which has mocked-out tab capture support.
    RenderWidgetHostView* old_view = GetView();
    SetView(new CaptureTestView(this, controller));
    delete old_view;
  }

  // TestRenderViewHost overrides.
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE {
    gfx::Size size = controller_->GetCopyResultSize();
    SkColor color = controller_->GetSolidColor();

    // Although it's not necessary, use a PlatformBitmap here (instead of a
    // regular SkBitmap) to exercise possible threading issues.
    skia::PlatformBitmap output;
    EXPECT_TRUE(output.Allocate(size.width(), size.height(), false));
    {
      SkAutoLockPixels locker(output.GetBitmap());
      output.GetBitmap().eraseColor(color);
    }
    callback.Run(true, output.GetBitmap());
    controller_->SignalBackingStoreCopy();
  }

 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHost);
};

#if defined(COMPILER_MSVC)
// Re-enable warning 4250
#pragma warning(pop)
#endif

class CaptureTestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit CaptureTestRenderViewHostFactory(
      CaptureTestSourceController* controller) : controller_(controller) {
    RegisterFactory(this);
  }

  virtual ~CaptureTestRenderViewHostFactory() {
    UnregisterFactory();
  }

  // RenderViewHostFactory implementation.
  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      bool swapped_out,
      SessionStorageNamespace* session_storage_namespace) {
    return new CaptureTestRenderViewHost(instance, delegate, widget_delegate,
                                         routing_id, swapped_out, controller_);
  }
 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHostFactory);
};

// A stub consumer of captured video frames, which checks the output of
// WebContentsVideoCaptureDevice.
class StubConsumer : public media::VideoCaptureDevice::EventHandler {
 public:
  StubConsumer() : error_encountered_(false), wait_color_(0xcafe1950) {}
  virtual ~StubConsumer() {}

  void QuitIfConditionMet(SkColor color) {
    base::AutoLock guard(lock_);

    if (wait_color_ == color || error_encountered_)
      MessageLoop::current()->Quit();
  }

  void WaitForNextColor(SkColor expected_color) {
    {
      base::AutoLock guard(lock_);
      wait_color_ = expected_color;
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_FALSE(error_encountered_);
    }
  }

  void WaitForError() {
    {
      base::AutoLock guard(lock_);
      wait_color_ = kNotInterested;
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_TRUE(error_encountered_);
    }
  }

  virtual void OnIncomingCapturedFrame(
      const uint8* data,
      int length,
      base::Time timestamp,
      int rotation,
      bool flip_vert,
      bool flip_horiz) OVERRIDE {
    DCHECK(data);
    static const int kNumPixels = kTestWidth * kTestHeight;
    EXPECT_EQ(kNumPixels * kBytesPerPixel, length);
    const uint32* p = reinterpret_cast<const uint32*>(data);
    const uint32* const p_end = p + kNumPixels;
    const SkColor color = *p;
    bool all_pixels_are_the_same_color = true;
    for (++p; p < p_end; ++p) {
      if (*p != color) {
        all_pixels_are_the_same_color = false;
        break;
      }
    }
    EXPECT_TRUE(all_pixels_are_the_same_color);
    PostColorOrError(color);
  }

  virtual void OnIncomingCapturedVideoFrame(media::VideoFrame* frame,
                                            base::Time timestamp) OVERRIDE {
    EXPECT_EQ(gfx::Size(kTestWidth, kTestHeight), frame->coded_size());
    EXPECT_EQ(media::VideoFrame::YV12, frame->format());
    uint8 yuv[3] = {0};
    for (int plane = 0; plane < 3; ++plane) {
      yuv[plane] = frame->data(plane)[0];
    }
    // TODO(nick): We just look at the first pixel presently, because if
    // the analysis is too slow, the backlog of frames will grow without bound
    // and trouble erupts. http://crbug.com/174519
    PostColorOrError(SkColorSetRGB(yuv[0], yuv[1], yuv[2]));
  }

  void PostColorOrError(SkColor new_color) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubConsumer::QuitIfConditionMet, base::Unretained(this), new_color));
  }

  virtual void OnError() OVERRIDE {
    {
      base::AutoLock guard(lock_);
      error_encountered_ = true;
    }
    PostColorOrError(kNothingYet);
  }

  virtual void OnFrameInfo(const media::VideoCaptureCapability& info) OVERRIDE {
    EXPECT_EQ(kTestWidth, info.width);
    EXPECT_EQ(kTestHeight, info.height);
    EXPECT_EQ(kTestFramesPerSecond, info.frame_rate);
    EXPECT_EQ(media::VideoCaptureCapability::kARGB, info.color);
  }

 private:
  base::Lock lock_;
  bool error_encountered_;
  SkColor wait_color_;

  DISALLOW_COPY_AND_ASSIGN(StubConsumer);
};

}  // namespace

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  WebContentsVideoCaptureDeviceTest() {}

 protected:
  virtual void SetUp() {
    // TODO(nick): Sadness and woe! Much "mock-the-world" boilerplate could be
    // eliminated here, if only we could use RenderViewHostTestHarness. The
    // catch is that we need our TestRenderViewHost to support a
    // CopyFromBackingStore operation that we control. To accomplish that,
    // either RenderViewHostTestHarness would have to support installing a
    // custom RenderViewHostFactory, or else we implant some kind of delegated
    // CopyFromBackingStore functionality into TestRenderViewHost itself.

    // The main thread will serve as the UI thread as well as the test thread.
    // We'll manually pump the run loop at appropriate times in the test.
    ui_thread_.reset(new TestBrowserThread(BrowserThread::UI, &message_loop_));

    render_process_host_factory_.reset(new MockRenderProcessHostFactory());
    // Create our (self-registering) RVH factory, so that when we create a
    // WebContents, it in turn creates CaptureTestRenderViewHosts.
    render_view_host_factory_.reset(
        new CaptureTestRenderViewHostFactory(&controller_));

    browser_context_.reset(new TestBrowserContext());

    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    static_cast<SiteInstanceImpl*>(site_instance.get())->
        set_render_process_host_factory(render_process_host_factory_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance));

    // This is actually a CaptureTestRenderViewHost.
    RenderWidgetHostImpl* rwh =
        RenderWidgetHostImpl::From(web_contents_->GetRenderViewHost());

    std::string device_id =
        WebContentsCaptureUtil::AppendWebContentsDeviceScheme(
            base::StringPrintf("%d:%d", rwh->GetProcess()->GetID(),
                               rwh->GetRoutingID()));

    base::Closure destroy_cb = base::Bind(
        &CaptureTestSourceController::OnShutdown,
        base::Unretained(&controller_));

    device_.reset(WebContentsVideoCaptureDevice::Create(device_id, destroy_cb));
  }

  virtual void TearDown() {
    // Tear down in opposite order of set-up.

    // The device is destroyed asynchronously, and will notify the
    // CaptureTestSourceController when it finishes destruction.
    // Trigger this, and wait.
    base::RunLoop shutdown_loop;
    controller_.SetShutdownHook(shutdown_loop.QuitClosure());
    device_->DeAllocate();
    device_.reset();
    shutdown_loop.Run();

    // Destroy the browser objects.
    web_contents_.reset();
    browser_context_.reset();

    content::RunAllPendingInMessageLoop();

    render_view_host_factory_.reset();
    render_process_host_factory_.reset();
  }

  // Accessors.
  CaptureTestSourceController* source() { return &controller_; }
  media::VideoCaptureDevice* device() { return device_.get(); }
  StubConsumer* consumer() { return &consumer_; }

 private:
  // The consumer is the ultimate recipient of captured pixel data.
  StubConsumer consumer_;

  // The controller controls which pixel patterns to produce.
  CaptureTestSourceController controller_;

  // We run the UI message loop on the main thread. The capture device
  // will also spin up its own threads.
  MessageLoopForUI message_loop_;
  scoped_ptr<TestBrowserThread> ui_thread_;

  // Self-registering RenderProcessHostFactory.
  scoped_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  // Creates capture-capable RenderViewHosts whose pixel content production is
  // under the control of |controller_|.
  scoped_ptr<CaptureTestRenderViewHostFactory> render_view_host_factory_;

  // A mocked-out browser and tab.
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<WebContents> web_contents_;

  // Finally, the WebContentsVideoCaptureDevice under test.
  scoped_ptr<media::VideoCaptureDevice> device_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDeviceTest);
};

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the SkBitmap and the VideoFrame
// paths, just as RenderWidgetHost might if the content falls in and out of
// accelerated compositing.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());

  device()->Start();

  bool use_video_frames = false;
  for (int i = 0; i < 4; i++, use_video_frames = !use_video_frames) {
    SCOPED_TRACE(StringPrintf("Using %s path, iteration #%d",
                              use_video_frames ? "VideoFrame" : "SkBitmap", i));
    source()->SetCanCopyToVideoFrame(use_video_frames);
    source()->SetSolidColor(SK_ColorRED);
    ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));
    source()->SetSolidColor(SK_ColorGREEN);
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorGREEN));
    source()->SetSolidColor(SK_ColorBLUE);
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorBLUE));
    source()->SetSolidColor(SK_ColorBLACK);
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorBLACK));
  }

  device()->DeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, RejectsInvalidAllocateParams) {
  device()->Allocate(1280, 720, -2, consumer());
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForError());
}

TEST_F(WebContentsVideoCaptureDeviceTest, BadFramesGoodFrames) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());

  // 1x1 is too small to process; we intend for this to result in an error.
  source()->SetCopyResultSize(1, 1);
  source()->SetSolidColor(SK_ColorRED);
  device()->Start();

  // These frames ought to be dropped during the Render stage. Let
  // several captures to happen.
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextBackingStoreCopy());

  // Now push some good frames through; they should be processed normally.
  source()->SetCopyResultSize(kTestWidth, kTestHeight);
  source()->SetSolidColor(SK_ColorGREEN);
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorGREEN));
  source()->SetSolidColor(SK_ColorRED);
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));

  device()->Stop();
  device()->DeAllocate();
}

}  // namespace content
