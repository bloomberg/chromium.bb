// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/fake_demuxer_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kNumFramesInOneConfig = 9;
static const int kNumFramesToReadFirst = 5;
static const int kNumConfigs = 3;
COMPILE_ASSERT(kNumFramesToReadFirst < kNumFramesInOneConfig,
               do_not_read_too_many_buffers);
COMPILE_ASSERT(kNumConfigs > 0, need_multiple_configs_to_trigger_config_change);

class FakeDemuxerStreamTest : public testing::Test {
 public:
  FakeDemuxerStreamTest()
      : status_(DemuxerStream::kAborted),
        read_pending_(false) {}
  virtual ~FakeDemuxerStreamTest() {}

  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK(read_pending_);
    read_pending_ = false;
    status_ = status;
    buffer_ = buffer;
  }

  enum ReadResult {
    OK,
    ABORTED,
    CONFIG_CHANGED,
    EOS,
    PENDING
  };

  void EnterNormalReadState() {
    stream_.reset(
        new FakeDemuxerStream(kNumConfigs, kNumFramesInOneConfig, false));
    for (int i = 0; i < kNumFramesToReadFirst; ++i)
      ReadAndExpect(OK);
  }

  void EnterBeforeConfigChangedState() {
    stream_.reset(
        new FakeDemuxerStream(kNumConfigs, kNumFramesInOneConfig, false));
    for (int i = 0; i < kNumFramesInOneConfig; ++i)
      ReadAndExpect(OK);
  }

  void EnterBeforeEOSState() {
    stream_.reset(new FakeDemuxerStream(1, kNumFramesInOneConfig, false));
    for (int i = 0; i < kNumFramesInOneConfig; ++i)
      ReadAndExpect(OK);
  }

  void ExpectReadResult(ReadResult result) {
    switch (result) {
      case OK:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kOk, status_);
        ASSERT_TRUE(buffer_);
        EXPECT_FALSE(buffer_->IsEndOfStream());
        break;

      case ABORTED:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kAborted, status_);
        EXPECT_FALSE(buffer_);
        break;

      case CONFIG_CHANGED:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kConfigChanged, status_);
        EXPECT_FALSE(buffer_);
        break;

      case EOS:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kOk, status_);
        ASSERT_TRUE(buffer_);
        EXPECT_TRUE(buffer_->IsEndOfStream());
        break;

      case PENDING:
        EXPECT_TRUE(read_pending_);
        break;
    }
  }

  void ReadAndExpect(ReadResult result) {
    EXPECT_FALSE(read_pending_);
    read_pending_ = true;
    stream_->Read(base::Bind(&FakeDemuxerStreamTest::BufferReady,
                             base::Unretained(this)));
    message_loop_.RunUntilIdle();
    ExpectReadResult(result);
  }

  void SatisfyReadAndExpect(ReadResult result) {
    EXPECT_TRUE(read_pending_);
    stream_->SatisfyRead();
    message_loop_.RunUntilIdle();
    ExpectReadResult(result);
  }

  void Reset() {
    bool had_read_pending = read_pending_;
    stream_->Reset();
    message_loop_.RunUntilIdle();

    EXPECT_FALSE(read_pending_);
    if (had_read_pending)
      ExpectReadResult(ABORTED);
  }

  void TestRead(int num_configs,
                int num_frames_in_one_config,
                bool is_encrypted) {
    stream_.reset(new FakeDemuxerStream(
        num_configs, num_frames_in_one_config, is_encrypted));

    const VideoDecoderConfig& config = stream_->video_decoder_config();
    EXPECT_TRUE(config.IsValidConfig());
    EXPECT_EQ(is_encrypted, config.is_encrypted());

    for (int i = 0; i < num_configs; ++i) {
      for (int j = 0; j < num_frames_in_one_config; ++j)
        ReadAndExpect(OK);

      if (i == num_configs - 1)
        ReadAndExpect(EOS);
      else
        ReadAndExpect(CONFIG_CHANGED);
    }

    // Will always get EOS after we hit EOS.
    ReadAndExpect(EOS);
  }

  base::MessageLoop message_loop_;
  scoped_ptr<FakeDemuxerStream> stream_;

  DemuxerStream::Status status_;
  scoped_refptr<DecoderBuffer> buffer_;
  bool read_pending_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStreamTest);
};

TEST_F(FakeDemuxerStreamTest, Read_OneConfig) {
  TestRead(1, 5, false);
}

TEST_F(FakeDemuxerStreamTest, Read_MultipleConfigs) {
  TestRead(3, 5, false);
}

TEST_F(FakeDemuxerStreamTest, Read_OneFramePerConfig) {
  TestRead(3, 1, false);
}

TEST_F(FakeDemuxerStreamTest, Read_Encrypted) {
  TestRead(6, 3, true);
}

TEST_F(FakeDemuxerStreamTest, HoldRead_Normal) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  SatisfyReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, HoldRead_BeforeConfigChanged) {
  EnterBeforeConfigChangedState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  SatisfyReadAndExpect(CONFIG_CHANGED);
}

TEST_F(FakeDemuxerStreamTest, HoldRead_BeforeEOS) {
  EnterBeforeEOSState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  SatisfyReadAndExpect(EOS);
}

TEST_F(FakeDemuxerStreamTest, Reset_Normal) {
  EnterNormalReadState();
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_AfterHoldRead) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_DuringPendingRead) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_BeforeConfigChanged) {
  EnterBeforeConfigChangedState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  Reset();
  ReadAndExpect(CONFIG_CHANGED);
}

TEST_F(FakeDemuxerStreamTest, Reset_BeforeEOS) {
  EnterBeforeEOSState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  Reset();
  ReadAndExpect(EOS);
}

}  // namespace media
