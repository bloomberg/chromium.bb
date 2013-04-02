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
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
namespace {
const int kTestWidth = 320;
const int kTestHeight = 240;
const int kTestFramesPerSecond = 20;
const base::TimeDelta kWaitTimeout = base::TimeDelta::FromMilliseconds(10000);
const SkColor kNothingYet = 0xdeadbeef;
const SkColor kNotInterested = ~kNothingYet;

void DeadlineExceeded(base::Closure quit_closure) {
  if (!base::debug::BeingDebugged()) {
    quit_closure.Run();
    FAIL() << "Deadline exceeded while waiting, quitting";
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

SkColor ConvertRgbToYuv(SkColor rgb) {
  uint8 yuv[3];
  media::ConvertRGB32ToYUV(reinterpret_cast<uint8*>(&rgb),
                           yuv, yuv + 1, yuv + 2, 1, 1, 1, 1, 1);
  return SkColorSetRGB(yuv[0], yuv[1], yuv[2]);
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
        can_copy_to_video_frame_(false),
        use_frame_subscriber_(false) {}

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

  void SignalCopy() {
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

  void SetUseFrameSubscriber(bool value) {
    base::AutoLock guard(lock_);
    use_frame_subscriber_ = value;
  }

  bool CanUseFrameSubscriber() {
    base::AutoLock guard(lock_);
    return use_frame_subscriber_;
  }

  void WaitForNextCopy() {
    {
      base::AutoLock guard(lock_);
      copy_done_ = MessageLoop::current()->QuitClosure();
    }

    RunCurrentLoopWithDeadline();
  }

 private:
  base::Lock lock_;  // Guards changes to all members.
  SkColor color_;
  gfx::Size copy_result_size_;
  bool can_copy_to_video_frame_;
  bool use_frame_subscriber_;
  base::Closure copy_done_;

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
    SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
    media::FillYUV(target, SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
    callback.Run(true);
    controller_->SignalCopy();
  }

  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE {
    subscriber_.reset(subscriber.release());
  }

  virtual void EndFrameSubscription() OVERRIDE {
    subscriber_.reset();
  }

  // Simulate a compositor paint event for our subscriber.
  void SimulateUpdate() {
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    scoped_refptr<media::VideoFrame> target;
    if (subscriber_ && subscriber_->ShouldCaptureFrame(&target, &callback)) {
      SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
      media::FillYUV(target, SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(callback, base::Time::Now(), true));
      controller_->SignalCopy();
    }
  }

 private:
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber_;
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
    controller_->SignalCopy();
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
      SessionStorageNamespace* session_storage_namespace) OVERRIDE {
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
  StubConsumer()
      : error_encountered_(false),
        wait_color_yuv_(0xcafe1950) {
    buffer_pool_ = new VideoCaptureBufferPool(
        gfx::Size(kTestWidth, kTestHeight), 2);
    EXPECT_TRUE(buffer_pool_->Allocate());
  }
  virtual ~StubConsumer() {}

  void QuitIfConditionMet(SkColor color) {
    base::AutoLock guard(lock_);

    if (wait_color_yuv_ == color || error_encountered_)
      MessageLoop::current()->Quit();
  }

  void WaitForNextColor(SkColor expected_color) {
    {
      base::AutoLock guard(lock_);
      wait_color_yuv_ = ConvertRgbToYuv(expected_color);
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
      wait_color_yuv_ = kNotInterested;
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_TRUE(error_encountered_);
    }
  }

  bool HasError() {
    base::AutoLock guard(lock_);
    return error_encountered_;
  }

  virtual scoped_refptr<media::VideoFrame> ReserveOutputBuffer() OVERRIDE {
    return buffer_pool_->ReserveForProducer(0);
  }

  virtual void OnIncomingCapturedFrame(
      const uint8* data,
      int length,
      base::Time timestamp,
      int rotation,
      bool flip_vert,
      bool flip_horiz) OVERRIDE {
    FAIL();
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp) OVERRIDE {
    EXPECT_EQ(gfx::Size(kTestWidth, kTestHeight), frame->coded_size());
    EXPECT_EQ(media::VideoFrame::YV12, frame->format());
    EXPECT_NE(0, buffer_pool_->RecognizeReservedBuffer(frame));
    uint8 yuv[3];
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
    EXPECT_EQ(media::VideoCaptureCapability::kI420, info.color);
  }

 private:
  base::Lock lock_;
  bool error_encountered_;
  SkColor wait_color_yuv_;
  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  DISALLOW_COPY_AND_ASSIGN(StubConsumer);
};

}  // namespace

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  WebContentsVideoCaptureDeviceTest() {}

  void ResetWebContents() {
    web_contents_.reset();
  }

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

    device_.reset(WebContentsVideoCaptureDevice::Create(device_id));

    content::RunAllPendingInMessageLoop();
  }

  virtual void TearDown() {
    // Tear down in opposite order of set-up.

    // The device is destroyed asynchronously, and will notify the
    // CaptureTestSourceController when it finishes destruction.
    // Trigger this, and wait.
    if (device_) {
      device_->DeAllocate();
      device_.reset();
    }

    content::RunAllPendingInMessageLoop();

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

  void SimulateDrawEvent() {
    if (source()->CanUseFrameSubscriber()) {
      // Print
      CaptureTestView* test_view = static_cast<CaptureTestView*>(
          web_contents_->GetRenderViewHost()->GetView());
      test_view->SimulateUpdate();
    } else {
      // Simulate a non-accelerated paint.
      NotificationService::current()->Notify(
          NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          Source<RenderWidgetHost>(web_contents_->GetRenderViewHost()),
          NotificationService::NoDetails());
    }
  }

  void DestroyVideoCaptureDevice() { device_.reset(); }

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

TEST_F(WebContentsVideoCaptureDeviceTest, InvalidInitialWebContentsError) {
  // Before the installs itself on the UI thread up to start capturing, we'll
  // delete the web contents. This should trigger an error which can happen in
  // practice; we should be able to recover gracefully.
  ResetWebContents();

  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());
  device()->Start();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForError());
  device()->DeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, WebContentsDestroyed) {
  // We'll simulate the tab being closed after the capture pipeline is up and
  // running.
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());
  device()->Start();

  // Do one capture to prove
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));

  content::RunAllPendingInMessageLoop();

  // Post a task to close the tab. We should see an error reported to the
  // consumer.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsVideoCaptureDeviceTest::ResetWebContents,
                 base::Unretained(this)));
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForError());
  device()->DeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest,
       StopDeviceBeforeCaptureMachineCreation) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());
  device()->Start();
  // Make a point of not running the UI messageloop here.
  device()->Stop();
  device()->DeAllocate();
  DestroyVideoCaptureDevice();

  // Currently, there should be CreateCaptureMachineOnUIThread() and
  // DestroyCaptureMachineOnUIThread() tasks pending on the current (UI) message
  // loop. These should both succeed without crashing, and the machine should
  // wind up in the idle state.
  content::RunAllPendingInMessageLoop();
}

TEST_F(WebContentsVideoCaptureDeviceTest, StopWithRendererWorkToDo) {
  // Set up the test to use RGB copies and an normal
  source()->SetCanCopyToVideoFrame(false);
  source()->SetUseFrameSubscriber(false);
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond,
                     consumer());
  device()->Start();
  // Make a point of not running the UI messageloop here.
  content::RunAllPendingInMessageLoop();

  for (int i = 0; i < 10; ++i)
    SimulateDrawEvent();

  device()->Stop();
  device()->DeAllocate();
  // Currently, there should be CreateCaptureMachineOnUIThread() and
  // DestroyCaptureMachineOnUIThread() tasks pending on the current message
  // loop. These should both succeed without crashing, and the machine should
  // wind up in the idle state.
  ASSERT_FALSE(consumer()->HasError());
  content::RunAllPendingInMessageLoop();
  ASSERT_FALSE(consumer()->HasError());
}

TEST_F(WebContentsVideoCaptureDeviceTest, DeviceRestart) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());
  device()->Start();
  content::RunAllPendingInMessageLoop();
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));
  SimulateDrawEvent();
  SimulateDrawEvent();
  source()->SetSolidColor(SK_ColorGREEN);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorGREEN));
  device()->Stop();

  // Device is stopped, but content can still be animating.
  SimulateDrawEvent();
  SimulateDrawEvent();
  content::RunAllPendingInMessageLoop();

  device()->Start();
  source()->SetSolidColor(SK_ColorBLUE);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorBLUE));
  source()->SetSolidColor(SK_ColorYELLOW);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorYELLOW));
  device()->DeAllocate();
}

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the three capture paths, simulating
// falling in and out of accelerated compositing.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond, consumer());

  device()->Start();

  for (int i = 0; i < 6; i++) {
    const char* name = NULL;
    switch (i % 3) {
      case 0:
        source()->SetCanCopyToVideoFrame(true);
        source()->SetUseFrameSubscriber(false);
        name = "VideoFrame";
        break;
      case 1:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(true);
        name = "Subscriber";
        break;
      case 2:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(false);
        name = "SkBitmap";
        break;
      default:
        FAIL();
    }

    SCOPED_TRACE(base::StringPrintf("Using %s path, iteration #%d", name, i));

    source()->SetSolidColor(SK_ColorRED);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));

    source()->SetSolidColor(SK_ColorGREEN);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorGREEN));

    source()->SetSolidColor(SK_ColorBLUE);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorBLUE));

    source()->SetSolidColor(SK_ColorBLACK);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorBLACK));
  }
  device()->DeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, RejectsInvalidAllocateParams) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&media::VideoCaptureDevice::Allocate,
                 base::Unretained(device()), 1280, 720, -2, consumer()));
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
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());

  // Now push some good frames through; they should be processed normally.
  source()->SetCopyResultSize(kTestWidth, kTestHeight);
  source()->SetSolidColor(SK_ColorGREEN);
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorGREEN));
  source()->SetSolidColor(SK_ColorRED);
  ASSERT_NO_FATAL_FAILURE(consumer()->WaitForNextColor(SK_ColorRED));

  device()->Stop();
  device()->DeAllocate();
}

// 60Hz sampled at 30Hz should produce 30Hz.
TEST(SmoothEventSamplerTest, Sample60HertzAt30Hertz) {
  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true);
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 60;

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "First timer event should sample.";
  sampler.RecordSample();
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "Should always be overdue until first paint.";

  // Steady state, we should capture every other vsync, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;

    ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 3, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state,
  // but at a different phase.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));

    ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));
  }
}

// 50Hz sampled at 30Hz should produce 25Hz.
TEST(SmoothEventSamplerTest, Sample50HertzAt30Hertz) {
  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true);
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 50;

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "First timer event should sample.";
  sampler.RecordSample();
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "Should always be overdue until first paint.";

  // Steady state, we should capture every other vsync, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;

    ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 2, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state,
  // but at a different phase.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));

    ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));
  }
}

// 30Hz sampled at 30Hz should produce 30Hz.
TEST(SmoothEventSamplerTest, Sample30HertzAt30Hertz) {
  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true);
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 30;

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "First timer event should sample.";
  sampler.RecordSample();
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "Should always be overdue until first paint.";

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 1, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));
  }
}

// 24Hz sampled at 30Hz should produce 24Hz.
TEST(SmoothEventSamplerTest, Sample24HertzAt30Hertz) {
  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true);
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 24;

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "First timer event should sample.";
  sampler.RecordSample();
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t))
      << "Should always be overdue until first paint.";

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 1, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    sampler.RecordSample();
    t += vsync;
    ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t));
  }
}

TEST(SmoothEventSamplerTest, DoubleDrawAtOneTimeStillDirties) {
  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true);
  const base::TimeDelta overdue_period = base::TimeDelta::FromSeconds(1);

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
  sampler.RecordSample();
  t += overdue_period;
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";

  // Now simulate 2 events with the same clock value.
  ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
  sampler.RecordSample();
  ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t))
      << "Two events at same time -- expected second not to be sampled.";
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t + overdue_period))
      << "Second event should dirty the capture state.";
  sampler.RecordSample();
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t + overdue_period));
}

TEST(SmoothEventSamplerTest, FallbackToPollingIfUpdatesUnreliable) {
  const base::TimeDelta timer_interval = base::TimeDelta::FromSeconds(1) / 30;
  SmoothEventSampler should_not_poll(timer_interval, true);
  SmoothEventSampler should_poll(timer_interval, false);

  base::Time t;
  ASSERT_TRUE(base::Time::FromString("Sat, 23 Mar 2013 1:21:08 GMT", &t));
  ASSERT_TRUE(should_not_poll.AddEventAndConsiderSampling(t));
  ASSERT_TRUE(should_poll.AddEventAndConsiderSampling(t));
  should_not_poll.RecordSample();
  should_poll.RecordSample();
  t += timer_interval;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_FALSE(should_poll.IsOverdueForSamplingAt(t))
      << "Dirty interval has not elapsed yet.";
  t += timer_interval;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_poll.RecordSample();
  t += timer_interval;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_poll.RecordSample();
  t += timer_interval / 3;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_poll.RecordSample();
}

}  // namespace content

