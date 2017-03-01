// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind_helpers.h"
#include "base/debug/debugger.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_frame_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video_capture_types.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {
namespace {

constexpr int kTestWidth = 320;
constexpr int kTestHeight = 240;
constexpr int kTestFramesPerSecond = 20;
constexpr float kTestDeviceScaleFactor = 2.0f;
constexpr SkColor kDrawColorNotSet = 0xcafe1950;
constexpr SkColor kWaitColorNotSet = ~kDrawColorNotSet;
constexpr SkColor kNothingYet = 0xdeadbeef;
constexpr SkColor kNotInterested = ~kNothingYet;

void DeadlineExceeded(base::Closure quit_closure) {
  if (!base::debug::BeingDebugged()) {
    if (!quit_closure.is_null())
      quit_closure.Run();
    FAIL() << "Deadline exceeded while waiting, quitting";
  } else {
    LOG(WARNING) << "Deadline exceeded; test would fail if debugger weren't "
                 << "attached.";
  }
}

void RunCurrentLoopWithDeadline() {
  base::Timer deadline(false, false);
  deadline.Start(
      FROM_HERE, TestTimeouts::action_max_timeout(),
      base::Bind(&DeadlineExceeded,
                 base::MessageLoop::current()->QuitWhenIdleClosure()));
  base::RunLoop().Run();
  deadline.Stop();
}

SkColor ConvertRgbToYuv(SkColor rgb) {
  uint8_t yuv[3];
  libyuv::ARGBToI420(reinterpret_cast<uint8_t*>(&rgb), 1, yuv, 1, yuv + 1, 1,
                     yuv + 2, 1, 1, 1);
  return SkColorSetRGB(yuv[0], yuv[1], yuv[2]);
}

media::VideoCaptureParams DefaultCaptureParams() {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  return capture_params;
}

// A stub implementation which fills solid-colors into VideoFrames in calls to
// CopyFromSurfaceToVideoFrame(). The colors are changed by tests in-between
// draw events to confirm that the right frames have the right content and in
// the right sequence.
class CaptureTestView : public TestRenderWidgetHostView {
 public:
  explicit CaptureTestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        fake_bounds_(100, 100, 100 + kTestWidth, 100 + kTestHeight),
        yuv_color_(kDrawColorNotSet) {}

  ~CaptureTestView() override {}

  // TestRenderWidgetHostView overrides.
  gfx::Rect GetViewBounds() const override {
    return fake_bounds_;
  }

  void SetSize(const gfx::Size& size) override {
    SetBounds(gfx::Rect(fake_bounds_.origin(), size));
  }

  void SetBounds(const gfx::Rect& rect) override {
    fake_bounds_ = rect;
  }

  bool IsSurfaceAvailableForCopy() const override { return true; }

  void CopyFromSurface(const gfx::Rect& src_rect,
                       const gfx::Size& output_size,
                       const ReadbackRequestCallback& callback,
                       const SkColorType color_type) override {
    // WebContentsVideoCaptureDevice implementation does not use this.
    NOTREACHED();
  }

  void CopyFromSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      scoped_refptr<media::VideoFrame> target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override {
    ASSERT_TRUE(src_subrect.IsEmpty());
    media::FillYUV(target.get(), SkColorGetR(yuv_color_),
                   SkColorGetG(yuv_color_), SkColorGetB(yuv_color_));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, gfx::Rect(), true));
  }

  void BeginFrameSubscription(
      std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber)
      override {
    subscriber_ = std::move(subscriber);
  }

  void EndFrameSubscription() override { subscriber_.reset(); }

  void SetSolidColor(SkColor rgb_color) {
    yuv_color_ = ConvertRgbToYuv(rgb_color);
  }

  // Simulate a compositor paint event for our subscriber.
  void SimulateUpdate() {
    const base::TimeTicks present_time = base::TimeTicks::Now();
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    scoped_refptr<media::VideoFrame> target;
    if (subscriber_ && subscriber_->ShouldCaptureFrame(
            gfx::Rect(), present_time, &target, &callback)) {
      CopyFromSurfaceToVideoFrame(gfx::Rect(), target,
                                  base::Bind(callback, present_time));
    }
  }

 private:
  std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber_;
  gfx::Rect fake_bounds_;
  SkColor yuv_color_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestView);
};

class CaptureTestRenderViewHost : public TestRenderViewHost {
 public:
  CaptureTestRenderViewHost(SiteInstance* instance,
                            RenderViewHostDelegate* delegate,
                            RenderWidgetHostDelegate* widget_delegate,
                            int32_t routing_id,
                            int32_t main_frame_routing_id,
                            bool swapped_out)
      : TestRenderViewHost(instance,
                           base::MakeUnique<RenderWidgetHostImpl>(
                               widget_delegate,
                               instance->GetProcess(),
                               routing_id,
                               false /* This means: "Is not hidden." */),
                           delegate,
                           main_frame_routing_id,
                           swapped_out) {
    // Override the default view installed by TestRenderViewHost; we need
    // our special subclass which has mocked-out tab capture support.
    RenderWidgetHostView* old_view = GetWidget()->GetView();
    GetWidget()->SetView(new CaptureTestView(GetWidget()));
    delete old_view;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHost);
};

class CaptureTestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  CaptureTestRenderViewHostFactory() { RegisterFactory(this); }

  ~CaptureTestRenderViewHostFactory() override { UnregisterFactory(); }

  // RenderViewHostFactory implementation.
  RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int32_t routing_id,
      int32_t main_frame_routing_id,
      bool swapped_out) override {
    return new CaptureTestRenderViewHost(instance, delegate, widget_delegate,
                                         routing_id, main_frame_routing_id,
                                         swapped_out);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptureTestRenderViewHostFactory);
};

// A stub consumer of captured video frames, which checks the output of
// WebContentsVideoCaptureDevice.
class StubClient : public media::VideoCaptureDevice::Client {
 public:
  StubClient(
      const base::Callback<void(SkColor, const gfx::Size&)>& report_callback,
      const base::Closure& error_callback)
      : report_callback_(report_callback),
        error_callback_(error_callback) {
    buffer_pool_ = new media::VideoCaptureBufferPoolImpl(
        base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(), 2);
  }
  ~StubClient() override {}

  MOCK_METHOD7(OnIncomingCapturedData,
               void(const uint8_t* data,
                    int length,
                    const media::VideoCaptureFormat& frame_format,
                    int rotation,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    int frame_feedback_id));

  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(OnStarted, void(void));

  media::VideoCaptureDevice::Client::Buffer ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage,
      int frame_feedback_id) override {
    CHECK_EQ(format, media::PIXEL_FORMAT_I420);
    int buffer_id_to_drop =
        media::VideoCaptureBufferPool::kInvalidId;  // Ignored.
    const int buffer_id = buffer_pool_->ReserveForProducer(
        dimensions, format, storage, frame_feedback_id, &buffer_id_to_drop);
    if (buffer_id == media::VideoCaptureBufferPool::kInvalidId)
      return media::VideoCaptureDevice::Client::Buffer();

    return media::VideoCaptureDeviceClient::MakeBufferStruct(
        buffer_pool_, buffer_id, frame_feedback_id);
  }

  // Trampoline method to workaround GMOCK problems with std::unique_ptr<>.
  void OnIncomingCapturedBuffer(Buffer buffer,
                                const media::VideoCaptureFormat& format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override {
    DoOnIncomingCapturedBuffer();
  }

  void OnIncomingCapturedBufferExt(
      media::VideoCaptureDevice::Client::Buffer buffer,
      const media::VideoCaptureFormat& format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const media::VideoFrameMetadata& additional_metadata) override {
    EXPECT_FALSE(visible_rect.IsEmpty());
    EXPECT_EQ(media::PIXEL_FORMAT_I420, format.pixel_format);
    EXPECT_EQ(kTestFramesPerSecond, format.frame_rate);

    // TODO(miu): We just look at the center pixel presently, because if the
    // analysis is too slow, the backlog of frames will grow without bound and
    // trouble erupts. http://crbug.com/174519
    using media::VideoFrame;
    auto buffer_access = buffer.handle_provider->GetHandleForInProcessAccess();
    auto frame = VideoFrame::WrapExternalSharedMemory(
        media::PIXEL_FORMAT_I420, format.frame_size, visible_rect,
        format.frame_size, buffer_access->data(), buffer_access->mapped_size(),
        base::SharedMemory::NULLHandle(), 0u, base::TimeDelta());
    const gfx::Point center = visible_rect.CenterPoint();
    const int center_offset_y =
        (frame->stride(VideoFrame::kYPlane) * center.y()) + center.x();
    const int center_offset_uv =
        (frame->stride(VideoFrame::kUPlane) * (center.y() / 2)) +
        (center.x() / 2);
    report_callback_.Run(
        SkColorSetRGB(frame->data(VideoFrame::kYPlane)[center_offset_y],
                      frame->data(VideoFrame::kUPlane)[center_offset_uv],
                      frame->data(VideoFrame::kVPlane)[center_offset_uv]),
        frame->visible_rect().size());
  }

  media::VideoCaptureDevice::Client::Buffer ResurrectLastOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage,
      int frame_feedback_id) override {
    CHECK_EQ(format, media::PIXEL_FORMAT_I420);
    const int buffer_id =
        buffer_pool_->ResurrectLastForProducer(dimensions, format, storage);
    if (buffer_id == media::VideoCaptureBufferPool::kInvalidId)
      return media::VideoCaptureDevice::Client::Buffer();
    return media::VideoCaptureDeviceClient::MakeBufferStruct(
        buffer_pool_, buffer_id, frame_feedback_id);
  }

  void OnError(const tracked_objects::Location& from_here,
               const std::string& reason) override {
    error_callback_.Run();
  }

  double GetBufferPoolUtilization() const override { return 0.0; }

 private:
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
  base::Callback<void(SkColor, const gfx::Size&)> report_callback_;
  base::Closure error_callback_;

  DISALLOW_COPY_AND_ASSIGN(StubClient);
};

class StubClientObserver {
 public:
  StubClientObserver()
      : error_encountered_(false),
        wait_color_yuv_(kWaitColorNotSet),
        expecting_frames_(true) {
    client_.reset(new StubClient(
        base::Bind(&StubClientObserver::DidDeliverFrame,
                   base::Unretained(this)),
        base::Bind(&StubClientObserver::OnError, base::Unretained(this))));
  }

  virtual ~StubClientObserver() {}

  std::unique_ptr<StubClient> PassClient() { return std::move(client_); }

  void SetIsExpectingFrames(bool expecting_frames) {
    expecting_frames_ = expecting_frames;
  }

  void QuitIfConditionsMet(SkColor color, const gfx::Size& size) {
    EXPECT_TRUE(expecting_frames_);
    if (error_encountered_ || wait_color_yuv_ == kNotInterested ||
        wait_color_yuv_ == color) {
      last_frame_color_yuv_ = color;
      last_frame_size_ = size;
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  // Run the current loop until the next frame is delivered.  Returns the YUV
  // color and frame size.
  std::pair<SkColor, gfx::Size> WaitForNextFrame() {
    wait_color_yuv_ = kNotInterested;
    error_encountered_ = false;

    RunCurrentLoopWithDeadline();

    CHECK(!error_encountered_);
    return std::make_pair(last_frame_color_yuv_, last_frame_size_);
  }

  // Run the current loop until a frame is delivered with the |expected_color|.
  void WaitForNextColor(SkColor expected_color) {
    wait_color_yuv_ = ConvertRgbToYuv(expected_color);
    error_encountered_ = false;

    RunCurrentLoopWithDeadline();

    ASSERT_FALSE(error_encountered_);
  }

  // Run the current loop until an error is encountered.
  void WaitForError() {
    wait_color_yuv_ = kNotInterested;
    error_encountered_ = false;

    RunCurrentLoopWithDeadline();

    ASSERT_TRUE(error_encountered_);
  }

  bool HasError() {
    return error_encountered_;
  }

  void OnError() {
    error_encountered_ = true;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionsMet,
        base::Unretained(this),
        kNothingYet,
        gfx::Size()));
  }

  void DidDeliverFrame(SkColor color, const gfx::Size& size) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionsMet,
        base::Unretained(this),
        color,
        size));
  }

 private:
  bool error_encountered_;
  SkColor wait_color_yuv_;
  SkColor last_frame_color_yuv_;
  gfx::Size last_frame_size_;
  std::unique_ptr<StubClient> client_;
  bool expecting_frames_;

  DISALLOW_COPY_AND_ASSIGN(StubClientObserver);
};

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  // This is public because C++ method pointer scoping rules are silly and make
  // this hard to use with Bind().
  void ResetWebContents() {
    web_contents_.reset();
  }

 protected:
  void SetUp() override {
    const display::Display test_display = test_screen_.GetPrimaryDisplay();
    display::Display display(test_display);
    display.set_id(0x1337);
    display.set_bounds(gfx::Rect(0, 0, 2560, 1440));
    display.set_device_scale_factor(kTestDeviceScaleFactor);
    test_screen_.display_list().RemoveDisplay(test_display.id());
    test_screen_.display_list().AddDisplay(display,
                                           display::DisplayList::Type::PRIMARY);
    display::Screen::SetScreenInstance(&test_screen_);
    ASSERT_EQ(&test_screen_, display::Screen::GetScreen());

    // TODO(nick): Sadness and woe! Much "mock-the-world" boilerplate could be
    // eliminated here, if only we could use RenderViewHostTestHarness. The
    // catch is that we need to inject our CaptureTestView::
    // CopyFromSurfaceToVideoFrame() mock. To accomplish that, either
    // RenderViewHostTestHarness would have to support installing a custom
    // RenderViewHostFactory, or else we implant some kind of delegated
    // CopyFromSurfaceToVideoFrame functionality into TestRenderWidgetHostView
    // itself.

    render_process_host_factory_.reset(new MockRenderProcessHostFactory());
    // Create our (self-registering) RVH factory, so that when we create a
    // WebContents, it in turn creates CaptureTestRenderViewHosts.
    render_view_host_factory_.reset(new CaptureTestRenderViewHostFactory());
    render_frame_host_factory_.reset(new TestRenderFrameHostFactory());

    browser_context_.reset(new TestBrowserContext());

    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    SiteInstanceImpl::set_render_process_host_factory(
        render_process_host_factory_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance.get()));
    RenderFrameHost* const main_frame = web_contents_->GetMainFrame();
    device_ = WebContentsVideoCaptureDevice::Create(base::StringPrintf(
        "web-contents-media-stream://%d:%d", main_frame->GetProcess()->GetID(),
        main_frame->GetRoutingID()));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Tear down in opposite order of set-up.

    if (device_) {
      device_->StopAndDeAllocate();
      device_.reset();
    }

    base::RunLoop().RunUntilIdle();

    // Destroy the browser objects.
    web_contents_.reset();
    browser_context_.reset();

    base::RunLoop().RunUntilIdle();

    SiteInstanceImpl::set_render_process_host_factory(NULL);
    render_frame_host_factory_.reset();
    render_view_host_factory_.reset();
    render_process_host_factory_.reset();

    display::Screen::SetScreenInstance(nullptr);
  }

  // Accessors.
  WebContents* web_contents() const { return web_contents_.get(); }
  CaptureTestView* test_view() const {
    return static_cast<CaptureTestView*>(
        web_contents_->GetRenderViewHost()->GetWidget()->GetView());
  }
  media::VideoCaptureDevice* device() { return device_.get(); }

  // Returns the device scale factor of the capture target's native view.
  float GetDeviceScaleFactor() const {
    RenderWidgetHostView* const view =
        web_contents_->GetRenderViewHost()->GetWidget()->GetView();
    CHECK(view);
    return ui::GetScaleFactorForNativeView(view->GetNativeView());
  }

  void SimulateDrawEvent() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Force at least one frame period's worth of time to pass. Otherwise,
    // internal logic may decide not to capture a frame because the draw events
    // are more frequent that kTestFramesPerSecond.
    //
    // TODO(miu): Instead of physically waiting, we should inject simulated
    // clocks for testing.
    base::RunLoop run_loop;
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        run_loop.QuitClosure(),
        base::TimeDelta::FromMicroseconds(
            base::Time::kMicrosecondsPerSecond / kTestFramesPerSecond));
    run_loop.Run();

    // Schedule the update to occur when the test runs the event loop (and not
    // before expectations have been set).
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&CaptureTestView::SimulateUpdate,
                                       base::Unretained(test_view())));
  }

  void SimulateSourceSizeChange(const gfx::Size& size) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* const view = test_view();
    view->SetSize(size);
    // Normally, RenderWidgetHostImpl would notify WebContentsImpl that the size
    // has changed.  However, in this test setup, where there is no render
    // process, we must notify WebContentsImpl directly.
    WebContentsImpl* const as_web_contents_impl =
        static_cast<WebContentsImpl*>(web_contents_.get());
    RenderWidgetHostDelegate* const as_rwh_delegate =
        static_cast<RenderWidgetHostDelegate*>(as_web_contents_impl);
    as_rwh_delegate->RenderWidgetWasResized(
        as_web_contents_impl->GetMainFrame()->GetRenderWidgetHost(), true);
  }

  // Repeatedly schedules draw events and scans for frames until the output from
  // the capture device matches the given RGB |color| and frame |size|.
  void SimulateDrawsUntilNewFrameSizeArrives(SkColor color,
                                             const gfx::Size& size) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    while ((base::TimeTicks::Now() - start_time) <
               TestTimeouts::action_max_timeout()) {
      SimulateDrawEvent();
      const auto color_and_size = client_observer()->WaitForNextFrame();
      if (color_and_size.first == ConvertRgbToYuv(color) &&
          color_and_size.second == size) {
        return;
      }
    }
    DeadlineExceeded(base::Closure());
  }

  void SimulateRefreshFrameRequest() {
    // Force at least three frame period's worth of time to pass. The wait is
    // needed because refresh frame requests are only honored when drawing
    // events, which trigger frame captures, are not occurring frequently
    // enough.
    //
    // TODO(miu): Instead of physically waiting, we should inject simulated
    // clocks for testing.
    base::RunLoop run_loop;
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        run_loop.QuitClosure(),
        base::TimeDelta::FromMicroseconds(
            3 * base::Time::kMicrosecondsPerSecond / kTestFramesPerSecond));
    run_loop.Run();

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&media::VideoCaptureDevice::RequestRefreshFrame,
                   base::Unretained(device_.get())));
  }

  void DestroyVideoCaptureDevice() { device_.reset(); }

  StubClientObserver* client_observer() {
    return &client_observer_;
  }

 private:
  display::test::TestScreen test_screen_;

  TestBrowserThreadBundle thread_bundle_;

  StubClientObserver client_observer_;

  // Self-registering RenderProcessHostFactory.
  std::unique_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  // Creates capture-capable RenderViewHosts.
  std::unique_ptr<CaptureTestRenderViewHostFactory> render_view_host_factory_;

  // Self-registering RenderFrameHostFactory.
  std::unique_ptr<TestRenderFrameHostFactory> render_frame_host_factory_;

  // A mocked-out browser and tab.
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<WebContents> web_contents_;

  // Finally, the WebContentsVideoCaptureDevice under test.
  std::unique_ptr<media::VideoCaptureDevice> device_;
};

// In real-world use cases, there can exist a race condition between starting
// capture on a WebContents and having the WebContents be destroyed in the
// meantime. This tests that WebContentsVideoCaptureDevice errors-out
// gracefully.
TEST_F(WebContentsVideoCaptureDeviceTest,
       SafelyStartsUpAfterWebContentsHasGone) {
  ResetWebContents();
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

// Tests that WebContentsVideoCaptureDevice starts, captures a frame, and then
// gracefully errors-out if the WebContents is destroyed before the device is
// stopped.
TEST_F(WebContentsVideoCaptureDeviceTest,
       RunsThenErrorsOutWhenWebContentsIsDestroyed) {
  // We'll simulate the tab being closed after the capture pipeline is up and
  // running.
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  // Do one capture to prove the tab is initially open and being captured
  // normally.
  test_view()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  base::RunLoop().RunUntilIdle();

  // Post a task to close the tab. We should see an error reported to the
  // consumer.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsVideoCaptureDeviceTest::ResetWebContents,
                 base::Unretained(this)));
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

// Sanity-check that starting/stopping the WebContentsVideoCaptureDevice, but
// without first returning to the event loop in-between, will behave rationally.
TEST_F(WebContentsVideoCaptureDeviceTest,
       StopDeviceBeforeCaptureMachineCreation) {
  device()->AllocateAndStart(DefaultCaptureParams(),
                             client_observer()->PassClient());

  // Make a point of not running the UI messageloop here.
  device()->StopAndDeAllocate();
  DestroyVideoCaptureDevice();

  // Currently, there should be CreateCaptureMachineOnUIThread() and
  // DestroyCaptureMachineOnUIThread() tasks pending on the current (UI) message
  // loop. These should both succeed without crashing, and the machine should
  // wind up in the idle state.
  base::RunLoop().RunUntilIdle();
}

// Tests that frames are delivered to different clients across restarts of the
// same instance.
TEST_F(WebContentsVideoCaptureDeviceTest,
       DeliversToCorrectClientAcrossRestarts) {
  // While the device is up-and-running, expect frame captures.
  client_observer()->SetIsExpectingFrames(true);
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));
  base::RunLoop().RunUntilIdle();
  test_view()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));
  SimulateDrawEvent();
  test_view()->SetSolidColor(SK_ColorGREEN);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  SimulateDrawEvent();
  device()->StopAndDeAllocate();

  base::RunLoop().RunUntilIdle();

  // Now that the device is stopped, expect frames are no longer captured.
  client_observer()->SetIsExpectingFrames(false);
  SimulateDrawEvent();
  SimulateDrawEvent();
  base::RunLoop().RunUntilIdle();

  // Re-start the device with a different client. Only the second client should
  // expect to see any frame captures.
  StubClientObserver observer2;
  observer2.SetIsExpectingFrames(true);
  auto client2 = observer2.PassClient();
  EXPECT_CALL(*client2, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client2));
  test_view()->SetSolidColor(SK_ColorBLUE);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorBLUE));
  test_view()->SetSolidColor(SK_ColorYELLOW);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorYELLOW));
  device()->StopAndDeAllocate();
}

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the RGB/SkBitmap and YUV/VideoFrame
// capture paths.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration #%d", i));

    test_view()->SetSolidColor(SK_ColorRED);
    for (int j = 0; j <= i; j++)
      SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

    test_view()->SetSolidColor(SK_ColorGREEN);
    for (int j = 0; j <= i; j++)
      SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));

    test_view()->SetSolidColor(SK_ColorBLUE);
    for (int j = 0; j <= i; j++)
      SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLUE));

    test_view()->SetSolidColor(SK_ColorBLACK);
    for (int j = 0; j <= i; j++)
      SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLACK));
  }

  device()->StopAndDeAllocate();
}

// Tests that, when configured with the FIXED_ASPECT_RATIO resolution change
// policy, the source size changes result in video frames of possibly varying
// resolutions, but all with the same aspect ratio.
TEST_F(WebContentsVideoCaptureDeviceTest, VariableResolution_FixedAspectRatio) {
  auto capture_params = DefaultCaptureParams();
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(capture_params, std::move(client));

  // Source size equals maximum size.  Expect delivered frames to be
  // kTestWidth by kTestHeight.
  test_view()->SetSolidColor(SK_ColorRED);
  const float device_scale_factor = GetDeviceScaleFactor();
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorRED, gfx::Size(kTestWidth, kTestHeight));

  // Source size is half in both dimensions.  Expect delivered frames to be of
  // the same aspect ratio as kTestWidth by kTestHeight, but larger than the
  // half size because the minimum height is 180 lines.
  test_view()->SetSolidColor(SK_ColorGREEN);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight / 2)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorGREEN, gfx::Size(180 * kTestWidth / kTestHeight, 180));

  // Source size changes aspect ratio.  Expect delivered frames to be padded
  // in the horizontal dimension to preserve aspect ratio.
  test_view()->SetSolidColor(SK_ColorBLUE);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorBLUE, gfx::Size(kTestWidth, kTestHeight));

  // Source size changes aspect ratio again.  Expect delivered frames to be
  // padded in the vertical dimension to preserve aspect ratio.
  test_view()->SetSolidColor(SK_ColorBLACK);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight / 2)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorBLACK, gfx::Size(kTestWidth, kTestHeight));

  device()->StopAndDeAllocate();
}

// Tests that, when configured with the ANY_WITHIN_LIMIT resolution change
// policy, the source size changes result in video frames of possibly varying
// resolutions.
TEST_F(WebContentsVideoCaptureDeviceTest, VariableResolution_AnyWithinLimits) {
  auto capture_params = DefaultCaptureParams();
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(capture_params, std::move(client));

  // Source size equals maximum size.  Expect delivered frames to be
  // kTestWidth by kTestHeight.
  test_view()->SetSolidColor(SK_ColorRED);
  const float device_scale_factor = GetDeviceScaleFactor();
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth, kTestHeight)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorRED, gfx::Size(kTestWidth, kTestHeight));

  // Source size is half in both dimensions.  Expect delivered frames to also
  // be half in both dimensions.
  test_view()->SetSolidColor(SK_ColorGREEN);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(
      device_scale_factor, gfx::Size(kTestWidth / 2, kTestHeight / 2)));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorGREEN, gfx::Size(kTestWidth / 2, kTestHeight / 2));

  // Source size changes to something arbitrary.  Since the source size is
  // less than the maximum size, expect delivered frames to be the same size
  // as the source size.
  test_view()->SetSolidColor(SK_ColorBLUE);
  gfx::Size arbitrary_source_size(kTestWidth / 2 + 42, kTestHeight - 10);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(device_scale_factor,
                                                 arbitrary_source_size));
  SimulateDrawsUntilNewFrameSizeArrives(SK_ColorBLUE, arbitrary_source_size);

  // Source size changes to something arbitrary that exceeds the maximum frame
  // size.  Since the source size exceeds the maximum size, expect delivered
  // frames to be downscaled.
  test_view()->SetSolidColor(SK_ColorBLACK);
  arbitrary_source_size = gfx::Size(kTestWidth * 2, kTestHeight / 2);
  SimulateSourceSizeChange(gfx::ConvertSizeToDIP(device_scale_factor,
                                                 arbitrary_source_size));
  SimulateDrawsUntilNewFrameSizeArrives(
      SK_ColorBLACK, gfx::Size(kTestWidth,
                               kTestWidth * arbitrary_source_size.height() /
                                   arbitrary_source_size.width()));

  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest,
       ComputesStandardResolutionsForPreferredSize) {
  // Helper function to run the same testing procedure for multiple combinations
  // of |policy|, |standard_size| and |oddball_size|.
  const auto RunTestForPreferredSize =
      [=](media::ResolutionChangePolicy policy,
          const gfx::Size& oddball_size,
          const gfx::Size& standard_size) {
    SCOPED_TRACE(::testing::Message()
                 << "policy=" << policy
                 << ", oddball_size=" << oddball_size.ToString()
                 << ", standard_size=" << standard_size.ToString());

    // Compute the expected preferred size.  For the fixed-resolution use case,
    // the |oddball_size| is always the expected size; whereas for the
    // variable-resolution cases, the |standard_size| is the expected size.
    // Also, adjust to account for the device scale factor.
    gfx::Size capture_preferred_size = gfx::ScaleToFlooredSize(
        policy == media::RESOLUTION_POLICY_FIXED_RESOLUTION ? oddball_size
                                                            : standard_size,
        1.0f / GetDeviceScaleFactor());
    ASSERT_NE(capture_preferred_size, web_contents()->GetPreferredSize());

    // Start the WebContentsVideoCaptureDevice.
    auto capture_params = DefaultCaptureParams();
    capture_params.requested_format.frame_size = oddball_size;
    capture_params.resolution_change_policy = policy;
    StubClientObserver unused_observer;
    auto client = unused_observer.PassClient();
    EXPECT_CALL(*client, OnStarted());
    device()->AllocateAndStart(capture_params, std::move(client));
    base::RunLoop().RunUntilIdle();

    // Check that the preferred size of the WebContents matches the one provided
    // by WebContentsVideoCaptureDevice.
    EXPECT_EQ(capture_preferred_size, web_contents()->GetPreferredSize());

    // Stop the WebContentsVideoCaptureDevice.
    device()->StopAndDeAllocate();
    base::RunLoop().RunUntilIdle();
  };

  const media::ResolutionChangePolicy policies[3] = {
    media::RESOLUTION_POLICY_FIXED_RESOLUTION,
    media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO,
    media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
  };

  for (size_t i = 0; i < arraysize(policies); ++i) {
    // A 16:9 standard resolution should be set as the preferred size when the
    // source size is almost or exactly 16:9.
    for (int delta_w = 0; delta_w <= +5; ++delta_w) {
      for (int delta_h = 0; delta_h <= +5; ++delta_h) {
        RunTestForPreferredSize(policies[i],
                                gfx::Size(1280 + delta_w, 720 + delta_h),
                                gfx::Size(1280, 720));
      }
    }
    for (int delta_w = -5; delta_w <= +5; ++delta_w) {
      for (int delta_h = -5; delta_h <= +5; ++delta_h) {
        RunTestForPreferredSize(policies[i],
                                gfx::Size(1365 + delta_w, 768 + delta_h),
                                gfx::Size(1280, 720));
      }
    }

    // A 4:3 standard resolution should be set as the preferred size when the
    // source size is almost or exactly 4:3.
    for (int delta_w = 0; delta_w <= +5; ++delta_w) {
      for (int delta_h = 0; delta_h <= +5; ++delta_h) {
        RunTestForPreferredSize(policies[i],
                                gfx::Size(640 + delta_w, 480 + delta_h),
                                gfx::Size(640, 480));
      }
    }
    for (int delta_w = -5; delta_w <= +5; ++delta_w) {
      for (int delta_h = -5; delta_h <= +5; ++delta_h) {
        RunTestForPreferredSize(policies[i],
                                gfx::Size(800 + delta_w, 600 + delta_h),
                                gfx::Size(768, 576));
      }
    }

    // When the source size is not a common video aspect ratio, there is no
    // adjustment made.
    RunTestForPreferredSize(
        policies[i], gfx::Size(1000, 1000), gfx::Size(1000, 1000));
    RunTestForPreferredSize(
        policies[i], gfx::Size(1600, 1000), gfx::Size(1600, 1000));
    RunTestForPreferredSize(
        policies[i], gfx::Size(837, 999), gfx::Size(837, 999));
  }
}

// Tests the Suspend/Resume() functionality.
TEST_F(WebContentsVideoCaptureDeviceTest, SuspendsAndResumes) {
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  for (int i = 0; i < 3; ++i) {
    // Draw a RED frame and wait for a normal frame capture to occur.
    test_view()->SetSolidColor(SK_ColorRED);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

    // Suspend capture and then draw a GREEN frame. No frame capture should
    // occur.
    device()->MaybeSuspend();
    base::RunLoop().RunUntilIdle();
    client_observer()->SetIsExpectingFrames(false);
    test_view()->SetSolidColor(SK_ColorGREEN);
    SimulateDrawEvent();
    base::RunLoop().RunUntilIdle();

    // Resume capture and expect a GREEN update frame to be captured.
    client_observer()->SetIsExpectingFrames(true);
    device()->Resume();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));

    // Draw a BLUE frame and wait for it to be captured.
    test_view()->SetSolidColor(SK_ColorBLUE);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLUE));
  }

  device()->StopAndDeAllocate();
}

// Tests the RequestRefreshFrame() functionality.
TEST_F(WebContentsVideoCaptureDeviceTest, ProvidesRefreshFrames) {
  auto client = client_observer()->PassClient();
  EXPECT_CALL(*client, OnStarted());
  device()->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  // Request a refresh frame before the first frame has been drawn.  This forces
  // a capture.
  test_view()->SetSolidColor(SK_ColorRED);
  SimulateRefreshFrameRequest();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  // Now, draw a frame and wait for a normal frame capture to occur.
  test_view()->SetSolidColor(SK_ColorGREEN);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));

  // Now, make three more refresh frame requests. Although the source has
  // changed to BLUE, no draw event has occurred. Therefore, expect the refresh
  // frames to contain the content from the last drawn frame, which is GREEN.
  test_view()->SetSolidColor(SK_ColorBLUE);
  for (int i = 0; i < 3; ++i) {
    SimulateRefreshFrameRequest();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  }

  device()->StopAndDeAllocate();
}

}  // namespace
}  // namespace content
