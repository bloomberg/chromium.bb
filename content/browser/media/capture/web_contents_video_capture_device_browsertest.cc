// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <stdint.h>

#include <array>
#include <cmath>
#include <tuple>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

namespace content {
namespace {

// Provides a fake representation of the entire video capture stack. It creates
// a VideoFrameReceiver that the device can deliver VideoFrames to, and adapts
// that to a simple callback structure that allows the browser tests to examine
// each video frame that was captured.
class FakeVideoCaptureStack {
 public:
  using FrameCallback =
      base::RepeatingCallback<void(scoped_refptr<media::VideoFrame> frame)>;
  void SetFrameCallback(FrameCallback callback) {
    frame_callback_ = std::move(callback);
  }

  std::unique_ptr<media::VideoFrameReceiver> CreateFrameReceiver() {
    return std::make_unique<FakeVideoFrameReceiver>(this);
  }

  bool started() const { return started_; }
  bool error_occurred() const { return error_occurred_; }

  void ExpectHasLogMessages() {
    EXPECT_FALSE(log_messages_.empty());
    while (!log_messages_.empty()) {
      VLOG(1) << "Next log message: " << log_messages_.front();
      log_messages_.pop_front();
    }
  }

  void ExpectNoLogMessages() {
    while (!log_messages_.empty()) {
      ADD_FAILURE() << "Unexpected log message: " << log_messages_.front();
      log_messages_.pop_front();
    }
  }

 private:
  // A minimal implementation of VideoFrameReceiver that wraps buffers into
  // VideoFrame instances and forwards all relevant callbacks and data to the
  // parent FakeVideoCaptureStack.
  class FakeVideoFrameReceiver : public media::VideoFrameReceiver {
   public:
    explicit FakeVideoFrameReceiver(FakeVideoCaptureStack* capture_stack)
        : capture_stack_(capture_stack) {}

   private:
    using Buffer = media::VideoCaptureDevice::Client::Buffer;

    void OnNewBufferHandle(
        int buffer_id,
        std::unique_ptr<Buffer::HandleProvider> handle_provider) final {
      buffers_[buffer_id] =
          handle_provider->GetHandleForInterProcessTransit(true);
    }

    void OnFrameReadyInBuffer(
        int buffer_id,
        int frame_feedback_id,
        std::unique_ptr<Buffer::ScopedAccessPermission> access,
        media::mojom::VideoFrameInfoPtr frame_info) final {
      const auto it = buffers_.find(buffer_id);
      CHECK(it != buffers_.end());
      mojo::ScopedSharedBufferHandle& buffer = it->second;

      const size_t mapped_size =
          media::VideoCaptureFormat(frame_info->coded_size, 0.0f,
                                    frame_info->pixel_format,
                                    frame_info->storage_type)
              .ImageAllocationSize();
      mojo::ScopedSharedBufferMapping mapping = buffer->Map(mapped_size);
      CHECK(mapping.get());

      auto frame = media::VideoFrame::WrapExternalData(
          frame_info->pixel_format, frame_info->coded_size,
          frame_info->visible_rect, frame_info->visible_rect.size(),
          reinterpret_cast<uint8_t*>(mapping.get()), mapped_size,
          frame_info->timestamp);
      CHECK(frame);
      if (frame_info->metadata) {
        frame->metadata()->MergeInternalValuesFrom(*frame_info->metadata);
      }
      // This destruction observer will unmap the shared memory when the
      // VideoFrame goes out-of-scope.
      frame->AddDestructionObserver(base::BindOnce(
          [](mojo::ScopedSharedBufferMapping mapping) {}, std::move(mapping)));
      // This destruction observer will notify the WebContentsVideoCaptureDevice
      // once all downstream code is done using the VideoFrame.
      frame->AddDestructionObserver(base::BindOnce(
          [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
          std::move(access)));

      capture_stack_->frame_callback_.Run(std::move(frame));
    }

    void OnBufferRetired(int buffer_id) final {
      const auto it = buffers_.find(buffer_id);
      CHECK(it != buffers_.end());
      buffers_.erase(it);
    }

    void OnError() final { capture_stack_->error_occurred_ = true; }

    void OnLog(const std::string& message) final {
      capture_stack_->log_messages_.push_back(message);
    }

    void OnStarted() final { capture_stack_->started_ = true; }

    void OnStartedUsingGpuDecode() final { NOTREACHED(); }

    FakeVideoCaptureStack* const capture_stack_;
    base::flat_map<int, mojo::ScopedSharedBufferHandle> buffers_;
  };

  FrameCallback frame_callback_;
  bool started_ = false;
  bool error_occurred_ = false;
  base::circular_deque<std::string> log_messages_;
};

class WebContentsVideoCaptureDeviceBrowserTest : public ContentBrowserTest {
 public:
  FakeVideoCaptureStack* capture_stack() { return &capture_stack_; }
  WebContentsVideoCaptureDevice* device() const { return device_.get(); }

  // Alters the solid fill color making up the page content. This will trigger a
  // compositor update, which will trigger a frame capture.
  void ChangePageContentColor(std::string css_color) {
    std::string script("document.body.style.backgroundColor = '#123456';");
    script.replace(script.find("#123456"), 7, css_color);
    CHECK(ExecuteScript(shell()->web_contents(), script));
  }

  // Returns the size of the WebContents top-level frame view.
  gfx::Size GetViewSize() const {
    return shell()
        ->web_contents()
        ->GetMainFrame()
        ->GetView()
        ->GetViewBounds()
        .size();
  }

  // Returns capture parameters based on the current size of the source view,
  // which is based on the size of the Shell window.
  media::VideoCaptureParams SnapshotCaptureParams() const {
    constexpr gfx::Size kMaxCaptureSize = gfx::Size(320, 320);
    constexpr int kMaxFramesPerSecond = 60;

    gfx::Size capture_size = kMaxCaptureSize;
    if (use_fixed_aspect_ratio()) {
      // Half either the width or height, depending on the source view size. The
      // goal is to force obvious letterboxing (or pillarboxing), regardless of
      // how the source view is currently sized.
      const gfx::Size view_size = GetViewSize();
      if (view_size.width() < view_size.height()) {
        capture_size.set_height(capture_size.height() / 2);
      } else {
        capture_size.set_width(capture_size.width() / 2);
      }
    }

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        capture_size, kMaxFramesPerSecond, media::PIXEL_FORMAT_I420,
        media::VideoPixelStorage::CPU);
    params.resolution_change_policy =
        use_fixed_aspect_ratio()
            ? media::ResolutionChangePolicy::FIXED_ASPECT_RATIO
            : media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
    return params;
  }

  // Creates and starts the device for frame capture, and checks that the
  // initial refresh frame is delivered.
  void AllocateAndStartAndWaitForFirstFrame() {
    frames_.clear();
    last_frame_timestamp_ = base::TimeDelta::Min();
    capture_stack()->SetFrameCallback(
        base::BindRepeating(&WebContentsVideoCaptureDeviceBrowserTest::OnFrame,
                            base::Unretained(this)));

    auto* const main_frame = shell()->web_contents()->GetMainFrame();
    device_ = std::make_unique<WebContentsVideoCaptureDevice>(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID());
    device_->AllocateAndStartWithReceiver(
        SnapshotCaptureParams(), capture_stack()->CreateFrameReceiver());
    RunAllPendingInMessageLoop(BrowserThread::UI);
    EXPECT_TRUE(capture_stack()->started());
    EXPECT_FALSE(capture_stack()->error_occurred());
    capture_stack()->ExpectNoLogMessages();

    min_capture_period_ = base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond /
        device_->capture_params().requested_format.frame_rate);
    WaitForFrameWithColor(SK_ColorBLACK);
  }

  // Stops and destroys the device.
  void StopAndDeAllocate() {
    device_->StopAndDeAllocate();
    RunAllPendingInMessageLoop(BrowserThread::UI);
    device_.reset();
  }

  void ClearCapturedFramesQueue() { frames_.clear(); }

  bool HasCapturedFramesInQueue() const { return !frames_.empty(); }

  // Runs the browser until a frame with the given |color| is found in the
  // captured frames queue, or until a testing failure has occurred.
  void WaitForFrameWithColor(SkColor color) {
    VLOG(1) << "Waiting for frame filled with color: red=" << SkColorGetR(color)
            << ", green=" << SkColorGetG(color)
            << ", blue=" << SkColorGetB(color);

    while (!testing::Test::HasFailure()) {
      EXPECT_TRUE(capture_stack()->started());
      EXPECT_FALSE(capture_stack()->error_occurred());
      capture_stack()->ExpectNoLogMessages();

      while (!frames_.empty() && !testing::Test::HasFailure()) {
        // Pop the next frame from the front of the queue and convert to a RGB
        // bitmap for analysis.
        SkBitmap rgb_frame = ConvertToSkBitmap(*(frames_.front()));
        frames_.pop_front();
        EXPECT_FALSE(rgb_frame.empty());

        // Analyze the frame and compute the average color value for both the
        // content and non-content (i.e., letterboxed) regions.
        const gfx::Size frame_size(rgb_frame.width(), rgb_frame.height());
        const gfx::Size current_view_size = GetViewSize();
        EXPECT_EQ(expected_view_size_, current_view_size)
            << "Sanity-check failed: View size changed sized during this test.";
        const gfx::Rect content_rect =
            use_fixed_aspect_ratio()
                ? media::ComputeLetterboxRegion(gfx::Rect(frame_size),
                                                current_view_size)
                : gfx::Rect(frame_size);
        std::array<double, 3> average_content_rgb;
        std::array<double, 3> average_letterbox_rgb;
        AnalyzeFrame(rgb_frame, content_rect, &average_content_rgb,
                     &average_letterbox_rgb);

        VLOG(1) << "Video frame analysis: size=" << frame_size.ToString()
                << ", expected content_rect=" << content_rect.ToString()
                << ", average_red=" << average_content_rgb[0]
                << ", average_green=" << average_content_rgb[1]
                << ", average_blue=" << average_content_rgb[2];

        // The letterboxed region should be black.
        if (use_fixed_aspect_ratio()) {
          EXPECT_NEAR(SkColorGetR(SK_ColorBLACK), average_letterbox_rgb[0],
                      kMaxColorDifference);
          EXPECT_NEAR(SkColorGetG(SK_ColorBLACK), average_letterbox_rgb[1],
                      kMaxColorDifference);
          EXPECT_NEAR(SkColorGetB(SK_ColorBLACK), average_letterbox_rgb[2],
                      kMaxColorDifference);
        }

        if (testing::Test::HasFailure()) {
          ADD_FAILURE() << "Test failure occurred at this frame; PNG dump: "
                        << cc::GetPNGDataUrl(rgb_frame);
          return;
        }

        if (IsApproximatelySameColor(color, average_content_rgb)) {
          VLOG(1) << "Observed desired frame.";
          return;
        } else {
          VLOG(3) << "PNG dump of undesired frame: "
                  << cc::GetPNGDataUrl(rgb_frame);
        }
      }

      // Wait for at least the minimum capture period before checking for more
      // captured frames.
      base::RunLoop run_loop;
      BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
                                     run_loop.QuitClosure(),
                                     min_capture_period_);
      run_loop.Run();
    }
  }

 protected:
  // These are overridden for the parameterized tests.
  virtual bool use_software_compositing() const { return false; }
  virtual bool use_fixed_aspect_ratio() const { return false; }

  void SetUp() override {
#if defined(OS_CHROMEOS)
    // Enable use of the GPU, where available, to greatly speed these tests up.
    // As of this writing, enabling this flag on ChromeOS bots with Mus enabled
    // causes a CHECK failure in GpuProcessTransportFactory::EstablishedGpuCh()
    // for a false "use_gpu_compositing".
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kMus)) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseGpuInTests);
    }
#else
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseGpuInTests);
#endif  // defined(OS_CHROMEOS)

    // Screen capture requires readback from compositor output.
    EnablePixelOutput();

    // Conditionally force software compositing instead of GPU-accelerated
    // compositing.
    if (use_software_compositing()) {
      UseSoftwareCompositing();
    }

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Set the page content to a solid fill color: Initially, black.
    static constexpr char kTestHtmlAsDataUrl[] =
        "data:text/html,<!doctype html>"
        "<body style='background-color: #000000;'></body>";
    ASSERT_TRUE(NavigateToURL(shell(), GURL(kTestHtmlAsDataUrl)));

    expected_view_size_ = GetViewSize();
    VLOG(1) << "View size is " << expected_view_size_.ToString();
  }

  void TearDownOnMainThread() override {
    frames_.clear();

    // Run any left-over tasks (usually these are delete-soon's and orphaned
    // tasks).
    base::RunLoop().RunUntilIdle();

    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  void OnFrame(scoped_refptr<media::VideoFrame> frame) {
    // Frame timestamps should be monotionically increasing.
    EXPECT_LT(last_frame_timestamp_, frame->timestamp());
    last_frame_timestamp_ = frame->timestamp();

    frames_.emplace_back(std::move(frame));
  }

  static SkBitmap ConvertToSkBitmap(const media::VideoFrame& frame) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(frame.visible_rect().width(),
                          frame.visible_rect().height());
    // TODO(miu): This is not Rec.709 colorspace conversion, and so will
    // introduce inaccuracies.
    libyuv::I420ToARGB(frame.visible_data(media::VideoFrame::kYPlane),
                       frame.stride(media::VideoFrame::kYPlane),
                       frame.visible_data(media::VideoFrame::kUPlane),
                       frame.stride(media::VideoFrame::kUPlane),
                       frame.visible_data(media::VideoFrame::kVPlane),
                       frame.stride(media::VideoFrame::kVPlane),
                       reinterpret_cast<uint8_t*>(bitmap.getPixels()),
                       static_cast<int>(bitmap.rowBytes()), bitmap.width(),
                       bitmap.height());
    return bitmap;
  }

  // Computes the average color of the content and letterboxed regions of the
  // frame.
  static void AnalyzeFrame(SkBitmap frame,
                           const gfx::Rect& content_rect,
                           std::array<double, 3>* average_content_rgb,
                           std::array<double, 3>* average_letterbox_rgb) {
    int64_t sum_of_content_values[3] = {0};
    int64_t sum_of_letterbox_values[3] = {0};
    for (int y = 0; y < frame.height(); ++y) {
      for (int x = 0; x < frame.width(); ++x) {
        const SkColor color = frame.getColor(x, y);
        int64_t* const sums = content_rect.Contains(x, y)
                                  ? sum_of_content_values
                                  : sum_of_letterbox_values;
        sums[0] += SkColorGetR(color);
        sums[1] += SkColorGetG(color);
        sums[2] += SkColorGetB(color);
      }
    }

    const double content_area =
        static_cast<double>(content_rect.size().GetArea());
    for (int i = 0; i < 3; ++i) {
      (*average_content_rgb)[i] =
          (content_area <= 0.0) ? NAN
                                : (sum_of_content_values[i] / content_area);
    }

    const double letterbox_area =
        (frame.width() * frame.height()) - content_area;
    for (int i = 0; i < 3; ++i) {
      (*average_letterbox_rgb)[i] =
          (letterbox_area <= 0.0)
              ? NAN
              : (sum_of_letterbox_values[i] / letterbox_area);
    }
  }

  static bool IsApproximatelySameColor(SkColor color,
                                       const std::array<double, 3> rgb) {
    const double r_diff = std::abs(SkColorGetR(color) - rgb[0]);
    const double g_diff = std::abs(SkColorGetG(color) - rgb[1]);
    const double b_diff = std::abs(SkColorGetB(color) - rgb[2]);
    return r_diff < kMaxColorDifference && g_diff < kMaxColorDifference &&
           b_diff < kMaxColorDifference;
  }

  FakeVideoCaptureStack capture_stack_;

  gfx::Size expected_view_size_;

  std::unique_ptr<WebContentsVideoCaptureDevice> device_;
  base::TimeDelta min_capture_period_;
  base::circular_deque<scoped_refptr<media::VideoFrame>> frames_;
  base::TimeDelta last_frame_timestamp_;

  static constexpr int kMaxColorDifference = 8;
};

// Tests that the device refuses to start if the WebContents target was
// destroyed before the device could start.
// TODO(crbug/754872): To be re-enabled in separate change.
IN_PROC_BROWSER_TEST_F(
    WebContentsVideoCaptureDeviceBrowserTest,
    DISABLED_ErrorsOutIfWebContentsHasGoneBeforeDeviceStart) {
  auto* const main_frame = shell()->web_contents()->GetMainFrame();
  const auto render_process_id = main_frame->GetProcess()->GetID();
  const auto render_frame_id = main_frame->GetRoutingID();
  const auto capture_params = SnapshotCaptureParams();

  // Delete the WebContents instance and the Shell. This makes the
  // render_frame_id invalid.
  shell()->web_contents()->Close();
  ASSERT_FALSE(RenderFrameHost::FromID(render_process_id, render_frame_id));

  // Create the device.
  auto device = std::make_unique<WebContentsVideoCaptureDevice>(
      render_process_id, render_frame_id);
  // Running the pending UI tasks should cause the device to realize the
  // WebContents is gone.
  RunAllPendingInMessageLoop(BrowserThread::UI);

  // Attempt to start the device, and expect the video capture stack to have
  // been notified of the error.
  device->AllocateAndStartWithReceiver(capture_params,
                                       capture_stack()->CreateFrameReceiver());
  EXPECT_FALSE(capture_stack()->started());
  EXPECT_TRUE(capture_stack()->error_occurred());
  capture_stack()->ExpectHasLogMessages();

  device->StopAndDeAllocate();
  RunAllPendingInMessageLoop(BrowserThread::UI);
}

// Tests that the device starts, captures a frame, and then gracefully
// errors-out because the WebContents is destroyed before the device is stopped.
// TODO(crbug/754872): To be re-enabled in separate change.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       DISABLED_ErrorsOutWhenWebContentsIsDestroyed) {
  AllocateAndStartAndWaitForFirstFrame();

  // Initially, the device captures any content changes normally.
  ChangePageContentColor("#ff0000");
  WaitForFrameWithColor(SK_ColorRED);

  // Delete the WebContents instance and the Shell, and allow the the "target
  // permanently lost" error to propagate to the video capture stack.
  shell()->web_contents()->Close();
  RunAllPendingInMessageLoop(BrowserThread::UI);
  EXPECT_TRUE(capture_stack()->error_occurred());
  capture_stack()->ExpectHasLogMessages();

  StopAndDeAllocate();
}

// Tests that the device stops delivering frames while suspended. When resumed,
// any content changes that occurred during the suspend should cause a new frame
// to be delivered, to ensure the client is up-to-date.
// TODO(crbug/754872): To be re-enabled in separate change.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       DISABLED_SuspendsAndResumes) {
  AllocateAndStartAndWaitForFirstFrame();

  // Initially, the device captures any content changes normally.
  ChangePageContentColor("#ff0000");
  WaitForFrameWithColor(SK_ColorRED);

  // Suspend the device.
  device()->MaybeSuspend();
  RunAllPendingInMessageLoop(BrowserThread::UI);
  ClearCapturedFramesQueue();

  // Change the page content and run the browser for five seconds. Expect no
  // frames were queued because the device should be suspended.
  ChangePageContentColor("#00ff00");
  base::RunLoop run_loop;
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
                                 run_loop.QuitClosure(),
                                 base::TimeDelta::FromSeconds(5));
  run_loop.Run();
  EXPECT_FALSE(HasCapturedFramesInQueue());

  // Resume the device and wait for an automatic refresh frame containing the
  // content that was updated while the device was suspended.
  device()->Resume();
  WaitForFrameWithColor(SK_ColorGREEN);

  StopAndDeAllocate();
}

// Tests that the device delivers refresh frames when asked, while the source
// content is not changing.
// TODO(crbug/754872): To be re-enabled in separate change.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       DISABLED_DeliversRefreshFramesUponRequest) {
  AllocateAndStartAndWaitForFirstFrame();

  // Set the page content to a known color.
  ChangePageContentColor("#ff0000");
  WaitForFrameWithColor(SK_ColorRED);

  // Without making any further changes to the source (which would trigger
  // frames to be captured), request and wait for ten refresh frames.
  for (int i = 0; i < 10; ++i) {
    ClearCapturedFramesQueue();
    device()->RequestRefreshFrame();
    WaitForFrameWithColor(SK_ColorRED);
  }

  StopAndDeAllocate();
}

class WebContentsVideoCaptureDeviceBrowserTestP
    : public WebContentsVideoCaptureDeviceBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool use_software_compositing() const override {
    return std::get<0>(GetParam());
  }
  bool use_fixed_aspect_ratio() const override {
    return std::get<1>(GetParam());
  }
};

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    ,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        // On ChromeOS, software compositing is not an option.
        testing::Values(false),
        // Force video frame resolutions to have a fixed aspect ratio?
        testing::Values(false, true)));
#else
INSTANTIATE_TEST_CASE_P(
    ,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        // Use software compositing instead of GPU-accelerated compositing?
        testing::Values(false, true),
        // Force video frame resolutions to have a fixed aspect ratio?
        testing::Values(false, true)));
#endif  // defined(OS_CHROMEOS)

// Tests that the device successfully captures a series of content changes,
// whether the browser is running with software compositing or GPU-accelerated
// compositing, and whether the WebContents is visible/hidden or
// occluded/unoccluded.
// TODO(crbug/754872): To be re-enabled in separate change.
IN_PROC_BROWSER_TEST_P(WebContentsVideoCaptureDeviceBrowserTestP,
                       DISABLED_CapturesContentChanges) {
  SCOPED_TRACE(testing::Message()
               << "Test parameters: "
               << (use_software_compositing() ? "Software Compositing"
                                              : "GPU Compositing")
               << " with "
               << (use_fixed_aspect_ratio() ? "Fixed Video Aspect Ratio"
                                            : "Variable Video Aspect Ratio"));

  AllocateAndStartAndWaitForFirstFrame();

  for (int visilibilty_case = 0; visilibilty_case < 3; ++visilibilty_case) {
    switch (visilibilty_case) {
      case 0:
        VLOG(1) << "Visibility case: WebContents is showing.";
        shell()->web_contents()->WasShown();
        base::RunLoop().RunUntilIdle();
        ASSERT_TRUE(shell()->web_contents()->IsVisible());
        break;
      case 1:
        VLOG(1) << "Visibility case: WebContents is hidden.";
        shell()->web_contents()->WasHidden();
        base::RunLoop().RunUntilIdle();
        ASSERT_FALSE(shell()->web_contents()->IsVisible());
        break;
      case 2:
        VLOG(1) << "Visibility case: WebContents is showing, but occluded.";
        shell()->web_contents()->WasShown();
        shell()->web_contents()->WasOccluded();
        base::RunLoop().RunUntilIdle();
        ASSERT_TRUE(shell()->web_contents()->IsVisible());
        break;
    }

    static const struct {
      const char* const css;
      SkColor skia;
    } kColorsToCycleThrough[] = {
        {"#ff0000", SK_ColorRED},   {"#00ff00", SK_ColorGREEN},
        {"#0000ff", SK_ColorBLUE},  {"#ffff00", SK_ColorYELLOW},
        {"#00ffff", SK_ColorCYAN},  {"#ff00ff", SK_ColorMAGENTA},
        {"#ffffff", SK_ColorWHITE},
    };
    for (const auto color : kColorsToCycleThrough) {
      ChangePageContentColor(color.css);
      WaitForFrameWithColor(color.skia);
    }
  }

  StopAndDeAllocate();
}

}  // namespace
}  // namespace content
