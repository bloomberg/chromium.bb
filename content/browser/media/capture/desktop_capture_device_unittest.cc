// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;
using ::testing::WithArg;

namespace content {

namespace {

MATCHER_P2(EqualsCaptureCapability, width, height, "") {
  return arg.width == width && arg.height == height;
}

const int kTestFrameWidth1 = 500;
const int kTestFrameHeight1 = 500;
const int kTestFrameWidth2 = 400;
const int kTestFrameHeight2 = 300;

const int kFrameRate = 30;

// The value of the padding bytes in unpacked frames.
const uint8_t kFramePaddingValue = 0;

// Use a special value for frame pixels to tell pixel bytes apart from the
// padding bytes in the unpacked frame test.
const uint8_t kFakePixelValue = 1;

// Use a special value for the first pixel to verify the result in the inverted
// frame test.
const uint8_t kFakePixelValueFirst = 2;

class MockDeviceClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD5(OnIncomingCapturedData,
               void(const uint8_t* data,
                    int length,
                    const media::VideoCaptureFormat& frame_format,
                    int rotation,
                    const base::TimeTicks& timestamp));
  MOCK_METHOD9(OnIncomingCapturedYuvData,
               void(const uint8_t* y_data,
                    const uint8_t* u_data,
                    const uint8_t* v_data,
                    size_t y_stride,
                    size_t u_stride,
                    size_t v_stride,
                    const media::VideoCaptureFormat& frame_format,
                    int clockwise_rotation,
                    const base::TimeTicks& timestamp));
  MOCK_METHOD0(DoReserveOutputBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD2(OnError,
               void(const tracked_objects::Location& from_here,
                    const std::string& reason));

  // Trampoline methods to workaround GMOCK problems with scoped_ptr<>.
  scoped_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override {
    EXPECT_TRUE(format == media::PIXEL_FORMAT_I420 &&
                storage == media::PIXEL_STORAGE_CPU);
    DoReserveOutputBuffer();
    return scoped_ptr<Buffer>();
  }
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const media::VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  void OnIncomingCapturedVideoFrame(
      scoped_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) override {
    DoOnIncomingCapturedVideoFrame();
  }

  double GetBufferPoolUtilization() const override { return 0.0; }
};

// Creates a DesktopFrame that has the first pixel bytes set to
// kFakePixelValueFirst, and the rest of the bytes set to kFakePixelValue, for
// UnpackedFrame and InvertedFrame verification.
webrtc::BasicDesktopFrame* CreateBasicFrame(const webrtc::DesktopSize& size) {
  webrtc::BasicDesktopFrame* frame = new webrtc::BasicDesktopFrame(size);
  DCHECK_EQ(frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel,
            frame->stride());
  memset(frame->data(),
         kFakePixelValue,
         frame->stride() * frame->size().height());
  memset(frame->data(),
         kFakePixelValueFirst,
         webrtc::DesktopFrame::kBytesPerPixel);
  return frame;
}

// DesktopFrame wrapper that flips wrapped frame upside down by inverting
// stride.
class InvertedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit InvertedDesktopFrame(webrtc::DesktopFrame* frame)
      : webrtc::DesktopFrame(
            frame->size(),
            -frame->stride(),
            frame->data() + (frame->size().height() - 1) * frame->stride(),
            frame->shared_memory()),
        original_frame_(frame) {
    set_dpi(frame->dpi());
    set_capture_time_ms(frame->capture_time_ms());
    mutable_updated_region()->Swap(frame->mutable_updated_region());
  }
  ~InvertedDesktopFrame() override {}

 private:
  scoped_ptr<webrtc::DesktopFrame> original_frame_;

  DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

// DesktopFrame wrapper that copies the input frame and doubles the stride.
class UnpackedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit UnpackedDesktopFrame(webrtc::DesktopFrame* frame)
      : webrtc::DesktopFrame(
            frame->size(),
            frame->stride() * 2,
            new uint8_t[frame->stride() * 2 * frame->size().height()],
            NULL) {
    memset(data(), kFramePaddingValue, stride() * size().height());
    CopyPixelsFrom(*frame,
                   webrtc::DesktopVector(),
                   webrtc::DesktopRect::MakeSize(size()));
    delete frame;
  }
  ~UnpackedDesktopFrame() override {
    delete[] data_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnpackedDesktopFrame);
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  FakeScreenCapturer()
      : callback_(NULL),
        frame_index_(0),
        generate_inverted_frames_(false),
        generate_cropped_frames_(false) {
  }
  ~FakeScreenCapturer() override {}

  void set_generate_inverted_frames(bool generate_inverted_frames) {
    generate_inverted_frames_ = generate_inverted_frames;
  }

  void set_generate_cropped_frames(bool generate_cropped_frames) {
    generate_cropped_frames_ = generate_cropped_frames;
  }

  // VideoFrameCapturer interface.
  void Start(Callback* callback) override { callback_ = callback; }

  void Capture(const webrtc::DesktopRegion& region) override {
    webrtc::DesktopSize size;
    if (frame_index_ % 2 == 0) {
      size = webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1);
    } else {
      size = webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2);
    }
    frame_index_++;

    webrtc::DesktopFrame* frame = CreateBasicFrame(size);

    if (generate_inverted_frames_) {
      frame = new InvertedDesktopFrame(frame);
    } else if (generate_cropped_frames_) {
      frame = new UnpackedDesktopFrame(frame);
    }
    callback_->OnCaptureCompleted(frame);
  }

  bool GetScreenList(ScreenList* screens) override { return false; }

  bool SelectScreen(webrtc::ScreenId id) override { return false; }

 private:
  Callback* callback_;
  int frame_index_;
  bool generate_inverted_frames_;
  bool generate_cropped_frames_;
};

// Helper used to check that only two specific frame sizes are delivered to the
// OnIncomingCapturedData() callback.
class FormatChecker {
 public:
  FormatChecker(const gfx::Size& size_for_even_frames,
                const gfx::Size& size_for_odd_frames)
      : size_for_even_frames_(size_for_even_frames),
        size_for_odd_frames_(size_for_odd_frames),
        frame_count_(0) {}

  void ExpectAcceptableSize(const media::VideoCaptureFormat& format) {
    if (frame_count_ % 2 == 0)
      EXPECT_EQ(size_for_even_frames_, format.frame_size);
    else
      EXPECT_EQ(size_for_odd_frames_, format.frame_size);
    ++frame_count_;
    EXPECT_EQ(kFrameRate, format.frame_rate);
    EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);
  }

 private:
  const gfx::Size size_for_even_frames_;
  const gfx::Size size_for_odd_frames_;
  int frame_count_;
};

}  // namespace

class DesktopCaptureDeviceTest : public testing::Test {
 public:
  void CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer> capturer) {
    capture_device_.reset(new DesktopCaptureDevice(
        std::move(capturer), DesktopMediaID::TYPE_SCREEN));
  }

  void CopyFrame(const uint8_t* frame, int size,
                 const media::VideoCaptureFormat&, int, base::TimeTicks) {
    ASSERT_TRUE(output_frame_.get() != NULL);
    ASSERT_EQ(output_frame_->stride() * output_frame_->size().height(), size);
    memcpy(output_frame_->data(), frame, size);
  }

 protected:
  scoped_ptr<DesktopCaptureDevice> capture_device_;
  scoped_ptr<webrtc::DesktopFrame> output_frame_;
};

// There is currently no screen capturer implementation for ozone. So disable
// the test that uses a real screen-capturer instead of FakeScreenCapturer.
// http://crbug.com/260318
#if defined(USE_OZONE)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
TEST_F(DesktopCaptureDeviceTest, MAYBE_Capture) {
  scoped_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::ScreenCapturer::Create(
          webrtc::DesktopCaptureOptions::CreateDefault()));
  CreateScreenCaptureDevice(std::move(capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_, _)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(SaveArg<1>(&frame_size),
            SaveArg<2>(&format),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_device_->AllocateAndStart(capture_params, std::move(client));
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device_->StopAndDeAllocate();

  EXPECT_GT(format.frame_size.width(), 0);
  EXPECT_GT(format.frame_size.height(), 0);
  EXPECT_EQ(kFrameRate, format.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);

  EXPECT_EQ(format.frame_size.GetArea() * 4, frame_size);
}

// Test that screen capturer behaves correctly if the source frame size changes
// but the caller cannot cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeConstantResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth1, kTestFrameHeight1));
  base::WaitableEvent done_event(false, false);

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_, _)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(WithArg<2>(Invoke(&format_checker,
                              &FormatChecker::ExpectAcceptableSize)),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least two frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes,
// where the video frames sent the the client vary in resolution but maintain
// the same aspect ratio.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeFixedAspectRatio) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(888, 500), gfx::Size(532, 300));
  base::WaitableEvent done_event(false, false);

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_,_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(WithArg<2>(Invoke(&format_checker,
                              &FormatChecker::ExpectAcceptableSize)),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  const gfx::Size high_def_16_by_9(1920, 1080);
  ASSERT_GE(high_def_16_by_9.width(),
            std::max(kTestFrameWidth1, kTestFrameWidth2));
  ASSERT_GE(high_def_16_by_9.height(),
            std::max(kTestFrameHeight1, kTestFrameHeight2));
  capture_params.requested_format.frame_size = high_def_16_by_9;
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least three frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes
// and the caller can cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeVariableResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth2, kTestFrameHeight2));
  base::WaitableEvent done_event(false, false);

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_,_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(WithArg<2>(Invoke(&format_checker,
                              &FormatChecker::ExpectAcceptableSize)),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  const gfx::Size high_def_16_by_9(1920, 1080);
  ASSERT_GE(high_def_16_by_9.width(),
            std::max(kTestFrameWidth1, kTestFrameWidth2));
  ASSERT_GE(high_def_16_by_9.height(),
            std::max(kTestFrameHeight1, kTestFrameHeight2));
  capture_params.requested_format.frame_size = high_def_16_by_9;
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least three frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// This test verifies that an unpacked frame is converted to a packed frame.
TEST_F(DesktopCaptureDeviceTest, UnpackedFrame) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();
  mock_capturer->set_generate_cropped_frames(true);
  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);

  int frame_size = 0;
  output_frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1)));

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_,_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(Invoke(this, &DesktopCaptureDeviceTest::CopyFrame),
            SaveArg<1>(&frame_size),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format =
      media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();

  // Verifies that |output_frame_| has the same data as a packed frame of the
  // same size.
  scoped_ptr<webrtc::BasicDesktopFrame> expected_frame(CreateBasicFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1)));
  EXPECT_EQ(output_frame_->stride() * output_frame_->size().height(),
            frame_size);
  EXPECT_EQ(
      0, memcmp(output_frame_->data(), expected_frame->data(), frame_size));
}

// The test verifies that a bottom-to-top frame is converted to top-to-bottom.
TEST_F(DesktopCaptureDeviceTest, InvertedFrame) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();
  mock_capturer->set_generate_inverted_frames(true);
  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);

  int frame_size = 0;
  output_frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1)));

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_,_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(Invoke(this, &DesktopCaptureDeviceTest::CopyFrame),
            SaveArg<1>(&frame_size),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();

  // Verifies that |output_frame_| has the same pixel values as the inverted
  // frame.
  scoped_ptr<webrtc::DesktopFrame> inverted_frame(
      new InvertedDesktopFrame(CreateBasicFrame(
          webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1))));
  EXPECT_EQ(output_frame_->stride() * output_frame_->size().height(),
            frame_size);
  for (int i = 0; i < output_frame_->size().height(); ++i) {
    EXPECT_EQ(0,
        memcmp(inverted_frame->data() + i * inverted_frame->stride(),
               output_frame_->data() + i * output_frame_->stride(),
               output_frame_->stride()));
  }
}

}  // namespace content
