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
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/session/phone/videoframe.h"

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

}  // namespace

class RTCVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const char* kUrl;
  static const PipelineStatistics kStatistics;

  RTCVideoDecoderTest() {
    decoder_ = new RTCVideoDecoder(&message_loop_, kUrl);
    renderer_ = new MockVideoRenderer();
    read_cb_ = base::Bind(&RTCVideoDecoderTest::FrameReady,
                          base::Unretained(this));

    DCHECK(decoder_);

    EXPECT_CALL(statistics_cb_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual ~RTCVideoDecoderTest() {
    // Finish up any remaining tasks.
    message_loop_.RunAllPending();
  }

  void InitializeDecoderSuccessfully() {
    // Test successful initialization.
    decoder_->Initialize(
        NULL, NewExpectedStatusCB(PIPELINE_OK), NewStatisticsCB());
    message_loop_.RunAllPending();
  }

  StatisticsCB NewStatisticsCB() {
    return base::Bind(&MockStatisticsCB::OnStatistics,
                      base::Unretained(&statistics_cb_));
  }

  MOCK_METHOD2(FrameReady, void(media::VideoDecoder::DecoderStatus status,
                                const scoped_refptr<media::VideoFrame>&));

  // Fixture members.
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
const char* RTCVideoDecoderTest::kUrl = "media://remote/0";
const PipelineStatistics RTCVideoDecoderTest::kStatistics;

TEST_F(RTCVideoDecoderTest, Initialize_Successful) {
  InitializeDecoderSuccessfully();

  // Test that the output media format is an uncompressed video surface that
  // matches the dimensions specified by RTC.
  EXPECT_EQ(kWidth, decoder_->natural_size().width());
  EXPECT_EQ(kHeight, decoder_->natural_size().height());
}

TEST_F(RTCVideoDecoderTest, DoReset) {
  InitializeDecoderSuccessfully();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk, _));
  decoder_->Read(read_cb_);
  decoder_->Reset(media::NewExpectedClosure());

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoRenderFrame) {
  InitializeDecoderSuccessfully();

  NullVideoFrame video_frame;

  for (size_t i = 0; i < media::limits::kMaxVideoFrames; ++i) {
    decoder_->RenderFrame(&video_frame);
  }

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoSetSize) {
  InitializeDecoderSuccessfully();

  int new_width = kWidth * 2;
  int new_height = kHeight * 2;
  gfx::Size new_natural_size(new_width, new_height);
  int new_reserved = 0;

  decoder_->SetSize(new_width, new_height, new_reserved);

  EXPECT_EQ(new_width, decoder_->natural_size().width());
  EXPECT_EQ(new_height, decoder_->natural_size().height());

  message_loop_.RunAllPending();
}
