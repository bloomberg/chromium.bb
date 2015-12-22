// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/test/frame_segmenter_for_test.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class AudioVideoPipelineDeviceTest;

namespace {

const base::TimeDelta kMonitorLoopDelay = base::TimeDelta::FromMilliseconds(20);

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
      .Append(FILE_PATH_LITERAL("test")).Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(name);
  return file_path;
}

class BufferFeeder : public MediaPipelineBackend::Decoder::Delegate {
 public:
  ~BufferFeeder() override {}

  static scoped_ptr<BufferFeeder> LoadAudio(MediaPipelineBackend* backend,
                                            const std::string& filename,
                                            const base::Closure& eos_cb);
  static scoped_ptr<BufferFeeder> LoadVideo(MediaPipelineBackend* backend,
                                            const std::string& filename,
                                            bool raw_h264,
                                            const base::Closure& eos_cb);

  bool eos() const { return eos_; }

  void Initialize(MediaPipelineBackend::Decoder* decoder,
                  const BufferList& buffers);
  void Start();

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override { ASSERT_TRUE(false); }
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {
    ASSERT_TRUE(false);
  }
  void OnVideoResolutionChanged(const Size& size) override {}

 private:
  explicit BufferFeeder(const base::Closure& eos_cb);
  void FeedBuffer();

  base::Closure eos_cb_;
  bool feeding_completed_;
  bool eos_;
  MediaPipelineBackend::Decoder* decoder_;
  BufferList buffers_;
  scoped_refptr<DecoderBufferBase> pending_buffer_;

  DISALLOW_COPY_AND_ASSIGN(BufferFeeder);
};

}  // namespace

class AudioVideoPipelineDeviceTest : public testing::Test {
 public:
  struct PauseInfo {
    PauseInfo() {}
    PauseInfo(base::TimeDelta d, base::TimeDelta l) : delay(d), length(l) {}
    ~PauseInfo() {}

    base::TimeDelta delay;
    base::TimeDelta length;
  };

  AudioVideoPipelineDeviceTest();
  ~AudioVideoPipelineDeviceTest() override;

  void SetUp() override {
    CastMediaShlib::Initialize(
        base::CommandLine::ForCurrentProcess()->argv());
  }

  void TearDown() override {
    // Pipeline must be destroyed before finalizing media shlib.
    backend_.reset();
    CastMediaShlib::Finalize();
  }

  void ConfigureForFile(const std::string& filename);
  void ConfigureForAudioOnly(const std::string& filename);
  void ConfigureForVideoOnly(const std::string& filename, bool raw_h264);

  // Pattern loops, waiting >= pattern[i].delay against media clock between
  // pauses, then pausing for >= pattern[i].length against MessageLoop
  // A pause with delay <0 signals to stop sequence and do not loop
  void SetPausePattern(const std::vector<PauseInfo> pattern);

  // Adds a pause to the end of pause pattern
  void AddPause(base::TimeDelta delay, base::TimeDelta length);

  void Start();
  void OnEndOfStream();

 private:
  void Initialize();

  void MonitorLoop();

  void OnPauseCompleted();

  scoped_ptr<TaskRunnerImpl> task_runner_;
  scoped_ptr<MediaPipelineBackend> backend_;
  scoped_ptr<BufferFeeder> audio_feeder_;
  scoped_ptr<BufferFeeder> video_feeder_;
  bool stopped_;

  // Current media time.
  base::TimeDelta pause_time_;

  // Pause settings
  std::vector<PauseInfo> pause_pattern_;
  int pause_pattern_idx_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineDeviceTest);
};

namespace {

BufferFeeder::BufferFeeder(const base::Closure& eos_cb)
    : eos_cb_(eos_cb), feeding_completed_(false), eos_(false) {
  CHECK(!eos_cb_.is_null());
}

void BufferFeeder::Initialize(MediaPipelineBackend::Decoder* decoder,
                              const BufferList& buffers) {
  CHECK(decoder);
  decoder_ = decoder;
  decoder_->SetDelegate(this);
  buffers_ = buffers;
  buffers_.push_back(scoped_refptr<DecoderBufferBase>(
      new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer())));
}

void BufferFeeder::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

void BufferFeeder::FeedBuffer() {
  // Possibly feed one buffer.
  CHECK(!buffers_.empty());
  if (feeding_completed_)
    return;

  pending_buffer_ = buffers_.front();
  BufferStatus status = decoder_->PushBuffer(pending_buffer_.get());
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);
  buffers_.pop_front();

  // Feeding is done, just wait for the end of stream callback.
  if (pending_buffer_->end_of_stream() || buffers_.empty()) {
    if (buffers_.empty() && !pending_buffer_->end_of_stream())
      LOG(WARNING) << "Stream emptied without feeding EOS frame";

    feeding_completed_ = true;
    return;
  }

  if (status == MediaPipelineBackend::kBufferPending)
    return;

  OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

void BufferFeeder::OnEndOfStream() {
  eos_ = true;
  eos_cb_.Run();
}

void BufferFeeder::OnPushBufferComplete(BufferStatus status) {
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);
  if (feeding_completed_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

// static
scoped_ptr<BufferFeeder> BufferFeeder::LoadAudio(MediaPipelineBackend* backend,
                                                 const std::string& filename,
                                                 const base::Closure& eos_cb) {
  CHECK(backend);
  base::FilePath file_path = GetTestDataFilePath(filename);
  DemuxResult demux_result = FFmpegDemuxForTest(file_path, true /* audio */);

  MediaPipelineBackend::AudioDecoder* decoder = backend->CreateAudioDecoder();
  CHECK(decoder);

  bool success = decoder->SetConfig(DecoderConfigAdapter::ToCastAudioConfig(
      kPrimary, demux_result.audio_config));
  CHECK(success);

  VLOG(2) << "Got " << demux_result.frames.size() << " audio input frames";
  scoped_ptr<BufferFeeder> feeder(new BufferFeeder(eos_cb));
  feeder->Initialize(decoder, demux_result.frames);
  return feeder;
}

// static
scoped_ptr<BufferFeeder> BufferFeeder::LoadVideo(MediaPipelineBackend* backend,
                                                 const std::string& filename,
                                                 bool raw_h264,
                                                 const base::Closure& eos_cb) {
  CHECK(backend);

  VideoConfig video_config;
  BufferList buffers;
  if (raw_h264) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    base::MemoryMappedFile video_stream;
    CHECK(video_stream.Initialize(file_path)) << "Couldn't open stream file: "
                                              << file_path.MaybeAsASCII();
    buffers = H264SegmenterForTest(video_stream.data(), video_stream.length());

    // TODO(erickung): Either pull data from stream or make caller specify value
    video_config.codec = kCodecH264;
    video_config.profile = kH264Main;
    video_config.additional_config = NULL;
    video_config.is_encrypted = false;
  } else {
    base::FilePath file_path = GetTestDataFilePath(filename);
    DemuxResult demux_result = FFmpegDemuxForTest(file_path, false /* audio */);
    buffers = demux_result.frames;
    video_config = DecoderConfigAdapter::ToCastVideoConfig(
        kPrimary, demux_result.video_config);
  }

  MediaPipelineBackend::VideoDecoder* decoder = backend->CreateVideoDecoder();
  CHECK(decoder);

  bool success = decoder->SetConfig(video_config);
  CHECK(success);

  VLOG(2) << "Got " << buffers.size() << " video input frames";
  scoped_ptr<BufferFeeder> feeder(new BufferFeeder(eos_cb));
  feeder->Initialize(decoder, buffers);
  return feeder;
}

}  // namespace

AudioVideoPipelineDeviceTest::AudioVideoPipelineDeviceTest()
    : stopped_(false), pause_pattern_() {}

AudioVideoPipelineDeviceTest::~AudioVideoPipelineDeviceTest() {}

void AudioVideoPipelineDeviceTest::AddPause(base::TimeDelta delay,
                                            base::TimeDelta length) {
  pause_pattern_.push_back(PauseInfo(delay, length));
}

void AudioVideoPipelineDeviceTest::SetPausePattern(
    const std::vector<PauseInfo> pattern) {
  pause_pattern_ = pattern;
}

void AudioVideoPipelineDeviceTest::ConfigureForAudioOnly(
    const std::string& filename) {
  Initialize();
  audio_feeder_ = BufferFeeder::LoadAudio(
      backend_.get(), filename,
      base::Bind(&AudioVideoPipelineDeviceTest::OnEndOfStream,
                 base::Unretained(this)));
  bool success = backend_->Initialize();
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::ConfigureForVideoOnly(
    const std::string& filename,
    bool raw_h264) {
  Initialize();
  video_feeder_ = BufferFeeder::LoadVideo(
      backend_.get(), filename, raw_h264,
      base::Bind(&AudioVideoPipelineDeviceTest::OnEndOfStream,
                 base::Unretained(this)));
  bool success = backend_->Initialize();
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::ConfigureForFile(
    const std::string& filename) {
  Initialize();
  base::Closure eos_cb = base::Bind(
      &AudioVideoPipelineDeviceTest::OnEndOfStream, base::Unretained(this));
  video_feeder_ = BufferFeeder::LoadVideo(backend_.get(), filename,
                                          false /* raw_h264 */, eos_cb);
  audio_feeder_ = BufferFeeder::LoadAudio(backend_.get(), filename, eos_cb);
  bool success = backend_->Initialize();
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::Start() {
  pause_time_ = base::TimeDelta();
  pause_pattern_idx_ = 0;
  stopped_ = false;

  if (audio_feeder_)
    audio_feeder_->Start();
  if (video_feeder_)
    video_feeder_->Start();

  backend_->Start(0);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AudioVideoPipelineDeviceTest::MonitorLoop,
                            base::Unretained(this)));
}

void AudioVideoPipelineDeviceTest::OnEndOfStream() {
  if ((!audio_feeder_ || audio_feeder_->eos()) &&
      (!video_feeder_ || video_feeder_->eos())) {
    bool success = backend_->Stop();
    stopped_ = true;
    ASSERT_TRUE(success);
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

void AudioVideoPipelineDeviceTest::MonitorLoop() {
  // Backend is stopped, no need to monitor the loop any more.
  if (stopped_)
    return;

  base::TimeDelta media_time =
      base::TimeDelta::FromMicroseconds(backend_->GetCurrentPts());

  if (!pause_pattern_.empty() &&
      pause_pattern_[pause_pattern_idx_].delay >= base::TimeDelta() &&
      media_time >= pause_time_ + pause_pattern_[pause_pattern_idx_].delay) {
    // Do Pause
    backend_->Pause();
    pause_time_ = base::TimeDelta::FromMicroseconds(backend_->GetCurrentPts());

    VLOG(2) << "Pausing at " << pause_time_.InMilliseconds() << "ms for " <<
        pause_pattern_[pause_pattern_idx_].length.InMilliseconds() << "ms";

    // Wait for pause finish
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&AudioVideoPipelineDeviceTest::OnPauseCompleted,
                              base::Unretained(this)),
        pause_pattern_[pause_pattern_idx_].length);
    return;
  }

  // Check state again in a little while
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&AudioVideoPipelineDeviceTest::MonitorLoop,
                            base::Unretained(this)),
      kMonitorLoopDelay);
}

void AudioVideoPipelineDeviceTest::OnPauseCompleted() {
  // Make sure the media time didn't move during that time.
  base::TimeDelta media_time =
      base::TimeDelta::FromMicroseconds(backend_->GetCurrentPts());

  // TODO(damienv):
  // Should be:
  // EXPECT_EQ(media_time, media_time_);
  // However, some backends, when rendering the first frame while in paused
  // mode moves the time forward.
  // This behaviour is not intended.
  EXPECT_GE(media_time, pause_time_);
  EXPECT_LE(media_time, pause_time_ + base::TimeDelta::FromMilliseconds(50));

  pause_time_ = media_time;
  pause_pattern_idx_ = (pause_pattern_idx_ + 1) % pause_pattern_.size();

  VLOG(2) << "Pause complete, restarting media clock";

  // Resume playback and frame feeding.
  backend_->Resume();

  MonitorLoop();
}

void AudioVideoPipelineDeviceTest::Initialize() {
  // Create the media device.
  task_runner_.reset(new TaskRunnerImpl());
  MediaPipelineDeviceParams params(task_runner_.get());
  backend_.reset(CastMediaShlib::CreateMediaPipelineBackend(params));
  CHECK(backend_);
}

TEST_F(AudioVideoPipelineDeviceTest, Mp3Playback) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  ConfigureForAudioOnly("sfx.mp3");
  Start();
  message_loop->Run();
}

TEST_F(AudioVideoPipelineDeviceTest, VorbisPlayback) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  ConfigureForAudioOnly("sfx.ogg");
  Start();
  message_loop->Run();
}

TEST_F(AudioVideoPipelineDeviceTest, H264Playback) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  ConfigureForVideoOnly("bear.h264", true /* raw_h264 */);
  Start();
  message_loop->Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlaybackWithPause) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  // Setup to pause for 100ms every 500ms
  AddPause(base::TimeDelta::FromMilliseconds(500),
           base::TimeDelta::FromMilliseconds(100));

  ConfigureForVideoOnly("bear-640x360.webm", false /* raw_h264 */);
  Start();
  message_loop->Run();
}

TEST_F(AudioVideoPipelineDeviceTest, Vp8Playback) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  ConfigureForVideoOnly("bear-vp8a.webm", false /* raw_h264 */);
  Start();
  message_loop->Run();
}

TEST_F(AudioVideoPipelineDeviceTest, WebmPlayback) {
  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  ConfigureForFile("bear-640x360.webm");
  Start();
  message_loop->Run();
}

}  // namespace media
}  // namespace chromecast
