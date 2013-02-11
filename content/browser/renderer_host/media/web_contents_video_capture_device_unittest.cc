// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"

#include "base/bind_helpers.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
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
const int kTestFramesPerSecond = 8;
const base::TimeDelta kWaitTimeout =
    base::TimeDelta::FromMilliseconds(2000);
const SkColor kNothingYet = 0xdeadbeef;
const SkColor kNotInterested = ~kNothingYet;
}

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromBackingStore().  The unit tests can change the color for successive
// captures.
class StubRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  StubRenderWidgetHost(RenderProcessHost* process, int routing_id)
      : RenderWidgetHostImpl(&delegate_, process, routing_id),
        color_(kNothingYet),
        copy_result_size_(kTestWidth, kTestHeight),
        copy_event_(false, false) {}

  void SetSolidColor(SkColor color) {
    base::AutoLock guard(lock_);
    color_ = color;
  }

  void SetCopyResultSize(int width, int height) {
    base::AutoLock guard(lock_);
    copy_result_size_ = gfx::Size(width, height);
  }

  bool WaitForNextBackingStoreCopy() {
    if (!copy_event_.TimedWait(kWaitTimeout)) {
      ADD_FAILURE() << "WaitForNextBackingStoreCopy: wait deadline exceeded";
      return false;
    }
    return true;
  }

  // RenderWidgetHostImpl overrides.
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE {
    // Although it's not necessary, use a PlatformBitmap here (instead of a
    // regular SkBitmap) to exercise possible threading issues.
    scoped_ptr<skia::PlatformBitmap> platform_bitmap(new skia::PlatformBitmap);
    EXPECT_TRUE(platform_bitmap->Allocate(
        copy_result_size_.width(), copy_result_size_.height(), false));
    {
      SkAutoLockPixels locker(platform_bitmap->GetBitmap());
      base::AutoLock guard(lock_);
      platform_bitmap->GetBitmap().eraseColor(color_);
    }

    callback.Run(true, platform_bitmap->GetBitmap());
    copy_event_.Signal();
  }

 private:
  class StubRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
   public:
    StubRenderWidgetHostDelegate() {}
    virtual ~StubRenderWidgetHostDelegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(StubRenderWidgetHostDelegate);
  };

  StubRenderWidgetHostDelegate delegate_;
  base::Lock lock_;  // Guards changes to color_.
  SkColor color_;
  gfx::Size copy_result_size_;
  base::WaitableEvent copy_event_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StubRenderWidgetHost);
};

// A stub consumer of captured video frames, which checks the output of
// WebContentsVideoCaptureDevice.
class StubConsumer : public media::VideoCaptureDevice::EventHandler {
 public:
  StubConsumer() : output_changed_(&lock_),
                   picture_color_(kNothingYet),
                   error_encountered_(false) {}
  virtual ~StubConsumer() {}

  // Returns false if an error was encountered.
  bool WaitForNextColorOrError(SkColor expected_color) {
    base::TimeTicks deadline = base::TimeTicks::Now() + kWaitTimeout;
    base::AutoLock guard(lock_);
    while (picture_color_ != expected_color && !error_encountered_) {
      output_changed_.TimedWait(kWaitTimeout);
      if (base::TimeTicks::Now() >= deadline) {
        ADD_FAILURE() << "WaitForNextColorOrError: wait deadline exceeded";
        return false;
      }
    }
    if (!error_encountered_) {
      EXPECT_EQ(expected_color, picture_color_);
      return true;
    } else {
      return false;
    }
  }

  virtual void OnIncomingCapturedFrame(const uint8* data, int length,
                                       base::Time timestamp) OVERRIDE {
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

    {
      base::AutoLock guard(lock_);
      if (color != picture_color_) {
        picture_color_ = color;
        output_changed_.Signal();
      }
    }
  }

  virtual void OnIncomingCapturedVideoFrame(media::VideoFrame* frame,
                                            base::Time timestamp) OVERRIDE {
    EXPECT_EQ(gfx::Size(kTestWidth, kTestHeight), frame->coded_size());
    EXPECT_EQ(media::VideoFrame::YV12, frame->format());
    bool all_pixels_are_the_same_color = true;
    uint8 yuv[3] = {0};
    for (int plane = 0; plane < 3; ++plane) {
      yuv[plane] = frame->data(plane)[0];
      for (int y = 0; y < frame->rows(plane); ++y) {
        for (int x = 0; x < frame->row_bytes(plane); ++x) {
          if (yuv[plane] != frame->data(plane)[x + y * frame->stride(plane)]) {
            all_pixels_are_the_same_color = false;
            break;
          }
        }
      }
    }
    EXPECT_TRUE(all_pixels_are_the_same_color);
    const SkColor color = SkColorSetRGB(yuv[0], yuv[1], yuv[2]);

    {
      base::AutoLock guard(lock_);
      if (color != picture_color_) {
        picture_color_ = color;
        output_changed_.Signal();
      }
    }
  }

  virtual void OnError() OVERRIDE {
    base::AutoLock guard(lock_);
    error_encountered_ = true;
    output_changed_.Signal();
  }

  virtual void OnFrameInfo(const media::VideoCaptureCapability& info) OVERRIDE {
    EXPECT_EQ(kTestWidth, info.width);
    EXPECT_EQ(kTestHeight, info.height);
    EXPECT_EQ(kTestFramesPerSecond, info.frame_rate);
    EXPECT_EQ(media::VideoCaptureCapability::kARGB, info.color);
  }

 private:
  base::Lock lock_;
  base::ConditionVariable output_changed_;
  SkColor picture_color_;
  bool error_encountered_;

  DISALLOW_COPY_AND_ASSIGN(StubConsumer);
};

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  WebContentsVideoCaptureDeviceTest() {}

 protected:
  virtual void SetUp() {
    // This is a MessageLoop for the current thread.  The MockRenderProcessHost
    // will schedule its destruction in this MessageLoop during TearDown().
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));

    // The CopyFromBackingStore and WebContents tracking occur on the UI thread.
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI));
    ui_thread_->Start();

    // And the rest...
    browser_context_.reset(new TestBrowserContext());
    source_.reset(new StubRenderWidgetHost(
        new MockRenderProcessHost(browser_context_.get()), MSG_ROUTING_NONE));
    destroyed_.reset(new base::WaitableEvent(true, false));
    device_.reset(WebContentsVideoCaptureDevice::CreateForTesting(
        source_.get(),
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(destroyed_.get()))));
    consumer_.reset(new StubConsumer);
  }

  virtual void TearDown() {
    // Tear down in opposite order of set-up.
    device_->DeAllocate();  // Guarantees no more use of consumer_.
    consumer_.reset();
    device_.reset();  // Release reference to internal CaptureMachine.
    message_loop_->RunUntilIdle();  // Just in case.
    destroyed_->Wait();  // Wait until CaptureMachine is fully destroyed.
    destroyed_.reset();
    source_.reset();
    browser_context_.reset();
    ui_thread_->Stop();
    ui_thread_.reset();
    message_loop_->RunUntilIdle();  // Deletes MockRenderProcessHost.
    message_loop_.reset();
  }

  // Accessors.
  StubRenderWidgetHost* source() const { return source_.get(); }
  media::VideoCaptureDevice* device() const { return device_.get(); }
  StubConsumer* consumer() const { return consumer_.get(); }

 private:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<StubRenderWidgetHost> source_;
  scoped_ptr<base::WaitableEvent> destroyed_;
  scoped_ptr<media::VideoCaptureDevice> device_;
  scoped_ptr<StubConsumer> consumer_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDeviceTest);
};

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the SkBitmap and the VideoFrame
// paths, just as RenderWidgetHost might if the content falls in and out of
// accelerated compositing.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond,
                     consumer());

  device()->Start();

  bool use_video_frames = false;
  for (int i = 0; i < 4; i++, use_video_frames = !use_video_frames) {
    SCOPED_TRACE(
        testing::Message() << "Using "
                           << (use_video_frames ? "VideoFrame" : "SkBitmap")
                           << " path, iteration #" << i);
    // TODO(nick): Implement this.
    // source()->SetUseVideoFrames(use_video_frames);
    source()->SetSolidColor(SK_ColorRED);
    source()->WaitForNextBackingStoreCopy();
    ASSERT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorRED));
    source()->SetSolidColor(SK_ColorGREEN);
    ASSERT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorGREEN));
    source()->SetSolidColor(SK_ColorBLUE);
    ASSERT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorBLUE));
    source()->SetSolidColor(SK_ColorBLACK);
    ASSERT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorBLACK));
  }

  device()->DeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, RejectsInvalidAllocateParams) {
  device()->Allocate(1280, 720, -2, consumer());
  EXPECT_FALSE(consumer()->WaitForNextColorOrError(kNotInterested));
}

TEST_F(WebContentsVideoCaptureDeviceTest, BadFramesGoodFrames) {
  device()->Allocate(kTestWidth, kTestHeight, kTestFramesPerSecond,
                     consumer());


  // 1x1 is too small to process; we intend for this to result in an error.
  source()->SetCopyResultSize(1, 1);
  source()->SetSolidColor(SK_ColorRED);
  device()->Start();

  // These frames ought to be dropped during the Render stage. Let
  // several captures to happen.
  ASSERT_TRUE(source()->WaitForNextBackingStoreCopy());
  ASSERT_TRUE(source()->WaitForNextBackingStoreCopy());
  ASSERT_TRUE(source()->WaitForNextBackingStoreCopy());
  ASSERT_TRUE(source()->WaitForNextBackingStoreCopy());
  ASSERT_TRUE(source()->WaitForNextBackingStoreCopy());

  // Now push some good frames through; they should be processed normally.
  source()->SetCopyResultSize(kTestWidth, kTestHeight);
  source()->SetSolidColor(SK_ColorGREEN);
  EXPECT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorGREEN));
  source()->SetSolidColor(SK_ColorRED);
  EXPECT_TRUE(consumer()->WaitForNextColorOrError(SK_ColorRED));
  device()->DeAllocate();
}

}  // namespace content
