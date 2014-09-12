// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/filters/demuxer_stream_adapter.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class DummyDemuxerStream : public ::media::DemuxerStream {
 public:
  // Creates a demuxer stream which provides frames either with a delay
  // or instantly. The scheduling pattern is the following:
  // - provides |delayed_frame_count| frames with a delay,
  // - then provides the following |cycle_count| - |delayed_frame_count|
  //   instantly,
  // - then provides |delayed_frame_count| frames with a delay,
  // - ... and so on.
  // Special cases:
  // - all frames are delayed: |delayed_frame_count| = |cycle_count|
  // - all frames are provided instantly: |delayed_frame_count| = 0
  // |config_idx| is a list of frame index before which there is
  // a change of decoder configuration.
  DummyDemuxerStream(int cycle_count,
                     int delayed_frame_count,
                     const std::list<int>& config_idx);
  virtual ~DummyDemuxerStream();

  // ::media::DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual ::media::AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual ::media::VideoDecoderConfig video_decoder_config() OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual bool SupportsConfigChanges() OVERRIDE;
  virtual ::media::VideoRotation video_rotation() OVERRIDE;

  bool has_pending_read() const {
    return has_pending_read_;
  }

 private:
  void DoRead(const ReadCB& read_cb);

  // Demuxer configuration.
  const int cycle_count_;
  const int delayed_frame_count_;
  std::list<int> config_idx_;

  // Number of frames sent so far.
  int frame_count_;

  bool has_pending_read_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxerStream);
};

DummyDemuxerStream::DummyDemuxerStream(
    int cycle_count,
    int delayed_frame_count,
    const std::list<int>& config_idx)
  : cycle_count_(cycle_count),
    delayed_frame_count_(delayed_frame_count),
    config_idx_(config_idx),
    frame_count_(0),
    has_pending_read_(false) {
  DCHECK_LE(delayed_frame_count, cycle_count);
}

DummyDemuxerStream::~DummyDemuxerStream() {
}

void DummyDemuxerStream::Read(const ReadCB& read_cb) {
  has_pending_read_ = true;
  if (!config_idx_.empty() && config_idx_.front() == frame_count_) {
    config_idx_.pop_front();
    has_pending_read_ = false;
    read_cb.Run(kConfigChanged,
                scoped_refptr< ::media::DecoderBuffer>());
    return;
  }

  if ((frame_count_ % cycle_count_) < delayed_frame_count_) {
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DummyDemuxerStream::DoRead, base::Unretained(this),
                   read_cb),
        base::TimeDelta::FromMilliseconds(20));
    return;
  }
  DoRead(read_cb);
}

::media::AudioDecoderConfig DummyDemuxerStream::audio_decoder_config() {
  LOG(FATAL) << "DummyDemuxerStream is a video DemuxerStream";
  return ::media::AudioDecoderConfig();
}

::media::VideoDecoderConfig DummyDemuxerStream::video_decoder_config() {
  gfx::Size coded_size(640, 480);
  gfx::Rect visible_rect(640, 480);
  gfx::Size natural_size(640, 480);
  return ::media::VideoDecoderConfig(
      ::media::kCodecH264,
      ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
      ::media::VideoFrame::YV12,
      coded_size,
      visible_rect,
      natural_size,
      NULL, 0,
      false);
}

::media::DemuxerStream::Type DummyDemuxerStream::type() {
  return VIDEO;
}

bool DummyDemuxerStream::SupportsConfigChanges() {
  return true;
}

::media::VideoRotation DummyDemuxerStream::video_rotation() {
  return ::media::VIDEO_ROTATION_0;
}

void DummyDemuxerStream::DoRead(const ReadCB& read_cb) {
  has_pending_read_ = false;
  scoped_refptr< ::media::DecoderBuffer> buffer(
      new ::media::DecoderBuffer(16));
  buffer->set_timestamp(frame_count_ * base::TimeDelta::FromMilliseconds(40));
  frame_count_++;
  read_cb.Run(kOk, buffer);
}

}  // namespace

class DemuxerStreamAdapterTest : public testing::Test {
 public:
  DemuxerStreamAdapterTest();
  virtual ~DemuxerStreamAdapterTest();

  void Initialize(::media::DemuxerStream* demuxer_stream);
  void Start();

 protected:
  void OnTestTimeout();
  void OnNewFrame(const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);
  void OnFlushCompleted();

  // Total number of frames to request.
  int total_frames_;

  // Number of demuxer read before issuing an early flush.
  int early_flush_idx_;
  bool use_post_task_for_flush_;

  // Number of expected read frames.
  int total_expected_frames_;

  // Number of frames actually read so far.
  int frame_received_count_;

  // List of expected frame indices with decoder config changes.
  std::list<int> config_idx_;

  scoped_ptr<DummyDemuxerStream> demuxer_stream_;

  scoped_ptr<CodedFrameProvider> coded_frame_provider_;

  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamAdapterTest);
};

DemuxerStreamAdapterTest::DemuxerStreamAdapterTest()
    : use_post_task_for_flush_(false) {
}

DemuxerStreamAdapterTest::~DemuxerStreamAdapterTest() {
}

void DemuxerStreamAdapterTest::Initialize(
    ::media::DemuxerStream* demuxer_stream) {
  coded_frame_provider_.reset(
      new DemuxerStreamAdapter(
          base::MessageLoopProxy::current(),
          scoped_refptr<BalancedMediaTaskRunnerFactory>(),
          demuxer_stream));
}

void DemuxerStreamAdapterTest::Start() {
  frame_received_count_ = 0;

  // TODO(damienv): currently, test assertions which fail do not trigger the
  // exit of the unit test, the message loop is still running. Find a different
  // way to exit the unit test.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapterTest::OnTestTimeout,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));

  coded_frame_provider_->Read(
      base::Bind(&DemuxerStreamAdapterTest::OnNewFrame,
                 base::Unretained(this)));
}

void DemuxerStreamAdapterTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

void DemuxerStreamAdapterTest::OnNewFrame(
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (video_config.IsValidConfig()) {
    ASSERT_GT(config_idx_.size(), 0);
    ASSERT_EQ(frame_received_count_, config_idx_.front());
    config_idx_.pop_front();
  }

  ASSERT_TRUE(buffer.get() != NULL);
  ASSERT_EQ(buffer->timestamp(),
            frame_received_count_ * base::TimeDelta::FromMilliseconds(40));
  frame_received_count_++;

  if (frame_received_count_ >= total_frames_) {
    coded_frame_provider_->Flush(
        base::Bind(&DemuxerStreamAdapterTest::OnFlushCompleted,
                   base::Unretained(this)));
    return;
  }

  coded_frame_provider_->Read(
      base::Bind(&DemuxerStreamAdapterTest::OnNewFrame,
                 base::Unretained(this)));

  ASSERT_LE(frame_received_count_, early_flush_idx_);
  if (frame_received_count_ == early_flush_idx_) {
    base::Closure flush_cb =
        base::Bind(&DemuxerStreamAdapterTest::OnFlushCompleted,
                   base::Unretained(this));
    if (use_post_task_for_flush_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&CodedFrameProvider::Flush,
                     base::Unretained(coded_frame_provider_.get()),
                     flush_cb));
    } else {
      coded_frame_provider_->Flush(flush_cb);
    }
    return;
  }
}

void DemuxerStreamAdapterTest::OnFlushCompleted() {
  ASSERT_EQ(frame_received_count_, total_expected_frames_);
  ASSERT_FALSE(demuxer_stream_->has_pending_read());
  base::MessageLoop::current()->QuitWhenIdle();
}

TEST_F(DemuxerStreamAdapterTest, NoDelay) {
  total_frames_ = 10;
  early_flush_idx_ = total_frames_;   // No early flush.
  total_expected_frames_ = 10;
  config_idx_.push_back(0);
  config_idx_.push_back(5);

  int cycle_count = 1;
  int delayed_frame_count = 0;
  demuxer_stream_.reset(
      new DummyDemuxerStream(
          cycle_count, delayed_frame_count, config_idx_));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  message_loop->Run();
}

TEST_F(DemuxerStreamAdapterTest, AllDelayed) {
  total_frames_ = 10;
  early_flush_idx_ = total_frames_;   // No early flush.
  total_expected_frames_ = 10;
  config_idx_.push_back(0);
  config_idx_.push_back(5);

  int cycle_count = 1;
  int delayed_frame_count = 1;
  demuxer_stream_.reset(
      new DummyDemuxerStream(
          cycle_count, delayed_frame_count, config_idx_));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  message_loop->Run();
}

TEST_F(DemuxerStreamAdapterTest, AllDelayedEarlyFlush) {
  total_frames_ = 10;
  early_flush_idx_ = 5;
  use_post_task_for_flush_ = true;
  total_expected_frames_ = 5;
  config_idx_.push_back(0);
  config_idx_.push_back(3);

  int cycle_count = 1;
  int delayed_frame_count = 1;
  demuxer_stream_.reset(
      new DummyDemuxerStream(
          cycle_count, delayed_frame_count, config_idx_));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  Initialize(demuxer_stream_.get());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapterTest::Start, base::Unretained(this)));
  message_loop->Run();
}

}  // namespace media
}  // namespace chromecast
