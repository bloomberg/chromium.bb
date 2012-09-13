// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include <deque>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Message;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::Invoke;
using media::MockStatisticsCB;
using media::MockVideoRenderer;
using media::NewExpectedClosure;
using media::NewExpectedStatusCB;
using media::PipelineStatistics;
using media::PIPELINE_OK;
using media::StatisticsCB;

namespace {

class NullVideoFrame : public cricket::VideoFrame {
 public:
  NullVideoFrame() {}
  virtual ~NullVideoFrame() {}

  virtual bool Reset(uint32 fourcc, int w, int h, int dw, int dh,
                     uint8 *sample, size_t sample_size,
                     size_t pixel_width, size_t pixel_height,
                     int64 elapsed_time, int64 time_stamp, int rotation)
                     OVERRIDE {
    return true;
  }

  virtual size_t GetWidth() const OVERRIDE { return 0; }
  virtual size_t GetHeight() const OVERRIDE { return 0; }
  virtual const uint8 *GetYPlane() const OVERRIDE { return NULL; }
  virtual const uint8 *GetUPlane() const OVERRIDE { return NULL; }
  virtual const uint8 *GetVPlane() const OVERRIDE { return NULL; }
  virtual uint8 *GetYPlane() OVERRIDE { return NULL; }
  virtual uint8 *GetUPlane() OVERRIDE { return NULL; }
  virtual uint8 *GetVPlane() OVERRIDE { return NULL; }
  virtual int32 GetYPitch() const OVERRIDE { return 0; }
  virtual int32 GetUPitch() const OVERRIDE { return 0; }
  virtual int32 GetVPitch() const OVERRIDE { return 0; }

  virtual size_t GetPixelWidth() const OVERRIDE { return 1; }
  virtual size_t GetPixelHeight() const OVERRIDE { return 1; }
  virtual int64 GetElapsedTime() const OVERRIDE { return 0; }
  virtual int64 GetTimeStamp() const OVERRIDE { return 0; }
  virtual void SetElapsedTime(int64 elapsed_time) OVERRIDE {}
  virtual void SetTimeStamp(int64 time_stamp) OVERRIDE {}

  virtual int GetRotation() const OVERRIDE { return 0; }

  virtual VideoFrame *Copy() const OVERRIDE { return NULL; }

  virtual bool MakeExclusive() OVERRIDE { return true; }

  virtual size_t CopyToBuffer(uint8 *buffer, size_t size) const OVERRIDE {
    return 0;
  }

  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8 *buffer,
                                    size_t size,
                                    int stride_rgb) const OVERRIDE {
    return 0;
  }

  virtual void StretchToPlanes(uint8 *y, uint8 *u, uint8 *v,
                               int32 pitchY, int32 pitchU, int32 pitchV,
                               size_t width, size_t height,
                               bool interpolate, bool crop) const OVERRIDE {
  }

  virtual size_t StretchToBuffer(size_t w, size_t h, uint8 *buffer, size_t size,
                                 bool interpolate, bool crop) const OVERRIDE {
    return 0;
  }

  virtual void StretchToFrame(VideoFrame *target, bool interpolate,
                              bool crop) const OVERRIDE {
  }

  virtual VideoFrame *Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const OVERRIDE {
    return NULL;
  }

 protected:
  virtual VideoFrame* CreateEmptyFrame(int w, int h,
                                       size_t pixel_width, size_t pixel_height,
                                       int64 elapsed_time,
                                       int64 time_stamp) const OVERRIDE {
    return NULL;
  }
};

class MockVideoTrack : public webrtc::VideoTrackInterface {
 public:
  static MockVideoTrack* Create() {
    return new talk_base::RefCountedObject<MockVideoTrack>();
  }

  virtual std::string kind() const OVERRIDE {
    NOTIMPLEMENTED();
    return "";
  }
  virtual std::string label() const OVERRIDE {
    NOTIMPLEMENTED();
    return "";
  }
  virtual bool enabled() const OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual TrackState state() const OVERRIDE {
    NOTIMPLEMENTED();
    return kEnded;
  }
  virtual bool set_enabled(bool enable) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool set_state(TrackState new_state) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void RegisterObserver(webrtc::ObserverInterface* observer) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void UnregisterObserver(
      webrtc::ObserverInterface* observer) OVERRIDE {
    NOTIMPLEMENTED();
  }
  MOCK_METHOD1(AddRenderer, void(webrtc::VideoRendererInterface* renderer));
  MOCK_METHOD1(RemoveRenderer, void(webrtc::VideoRendererInterface* renderer));

  virtual cricket::VideoRenderer* FrameInput() OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

 protected:
  MockVideoTrack() {}
  ~MockVideoTrack() {}
};

}  // namespace

class RTCVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const PipelineStatistics kStatistics;

  RTCVideoDecoderTest() {
  }

  virtual ~RTCVideoDecoderTest() {
  }

  virtual void SetUp() OVERRIDE {
    video_track_ = MockVideoTrack::Create();
    decoder_ = new RTCVideoDecoder(message_loop_.message_loop_proxy(),
                                   message_loop_.message_loop_proxy(),
                                   video_track_);
    renderer_ = new MockVideoRenderer();
    read_cb_ = base::Bind(&RTCVideoDecoderTest::FrameReady,
                          base::Unretained(this));

    DCHECK(decoder_);

    EXPECT_CALL(statistics_cb_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual void TearDown() OVERRIDE {
    if (decoder_->state_ == RTCVideoDecoder::kStopped)
      return;
    Stop();
  }

  void InitializeDecoderSuccessfully() {
    EXPECT_CALL(*video_track_, AddRenderer(decoder_.get()));
    // Test successful initialization.
    decoder_->Initialize(
        NULL, NewExpectedStatusCB(PIPELINE_OK), NewStatisticsCB());
    message_loop_.RunAllPending();
  }

  void Stop() {
    EXPECT_CALL(*video_track_, RemoveRenderer(decoder_.get()));
    decoder_->Stop(media::NewExpectedClosure());

    message_loop_.RunAllPending();
    EXPECT_EQ(RTCVideoDecoder::kStopped, decoder_->state_);
  }

  StatisticsCB NewStatisticsCB() {
    return base::Bind(&MockStatisticsCB::OnStatistics,
                      base::Unretained(&statistics_cb_));
  }

  void RenderFrame() {
    NullVideoFrame video_frame;
    decoder_->RenderFrame(&video_frame);
  }

  MOCK_METHOD2(FrameReady, void(media::VideoDecoder::Status status,
                                const scoped_refptr<media::VideoFrame>&));

  // Fixture members.
  scoped_refptr<MockVideoTrack> video_track_;
  scoped_refptr<RTCVideoDecoder> decoder_;
  scoped_refptr<MockVideoRenderer> renderer_;
  MockStatisticsCB statistics_cb_;
  MessageLoop message_loop_;
  media::VideoDecoder::ReadCB read_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderTest);
};

const int RTCVideoDecoderTest::kWidth = 640;
const int RTCVideoDecoderTest::kHeight = 480;
const PipelineStatistics RTCVideoDecoderTest::kStatistics;

MATCHER_P2(HasSize, width, height, "") {
  EXPECT_EQ(arg->data_size().width(), width);
  EXPECT_EQ(arg->data_size().height(), height);
  EXPECT_EQ(arg->natural_size().width(), width);
  EXPECT_EQ(arg->natural_size().height(), height);
  return (arg->data_size().width() == width) &&
      (arg->data_size().height() == height) &&
      (arg->natural_size().width() == width) &&
      (arg->natural_size().height() == height);
}

TEST_F(RTCVideoDecoderTest, Initialize_Successful) {
  InitializeDecoderSuccessfully();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                HasSize(kWidth, kHeight)));
  decoder_->Read(read_cb_);
  RenderFrame();
}

TEST_F(RTCVideoDecoderTest, DoReset) {
  InitializeDecoderSuccessfully();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                scoped_refptr<media::VideoFrame>()));
  decoder_->Read(read_cb_);
  decoder_->Reset(media::NewExpectedClosure());

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoRenderFrame) {
  InitializeDecoderSuccessfully();

  for (size_t i = 0; i < media::limits::kMaxVideoFrames; ++i)
    RenderFrame();

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoSetSize) {
  InitializeDecoderSuccessfully();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                HasSize(kWidth, kHeight)));
  decoder_->Read(read_cb_);
  RenderFrame();
  message_loop_.RunAllPending();

  int new_width = kWidth * 2;
  int new_height = kHeight * 2;
  decoder_->SetSize(new_width, new_height);

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                HasSize(new_width, new_height)));
  decoder_->Read(read_cb_);
  RenderFrame();
  message_loop_.RunAllPending();
}

TEST_F(RTCVideoDecoderTest, ReadAndShutdown) {
  // Test all the Read requests can be fullfilled (which is needed in order to
  // teardown the pipeline) even when there's no input frame.
  InitializeDecoderSuccessfully();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                scoped_refptr<media::VideoFrame>()));
  decoder_->Read(read_cb_);
  Stop();

  // Any read after stopping should be immediately satisfied.
  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                scoped_refptr<media::VideoFrame>()));
  decoder_->Read(read_cb_);
  message_loop_.RunAllPending();
}
