// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
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

}  // namespace

class AudioVideoPipelineDeviceTest : public testing::Test,
                                     public MediaPipelineBackend::Delegate {
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

  // MediaPipelineBackend::Delegate implementation:
  void OnVideoResolutionChanged(MediaPipelineBackend::VideoDecoder* decoder,
                                const Size& size) override {}
  void OnPushBufferComplete(MediaPipelineBackend::Decoder* decoder,
                            MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream(MediaPipelineBackend::Decoder* decoder) override;
  void OnDecoderError(MediaPipelineBackend::Decoder* decoder) override;

 private:
  void Initialize();

  void LoadAudioStream(const std::string& filename);
  void LoadVideoStream(const std::string& filename, bool raw_h264);

  void FeedAudioBuffer();
  void FeedVideoBuffer();

  void MonitorLoop();

  void OnPauseCompleted();

  scoped_ptr<TaskRunnerImpl> task_runner_;
  scoped_ptr<MediaPipelineBackend> backend_;
  scoped_refptr<DecoderBufferBase> backend_audio_buffer_;
  scoped_refptr<DecoderBufferBase> backend_video_buffer_;

  // Current media time.
  base::TimeDelta pause_time_;

  // Pause settings
  std::vector<PauseInfo> pause_pattern_;
  int pause_pattern_idx_;

  BufferList audio_buffers_;
  BufferList video_buffers_;

  MediaPipelineBackend::AudioDecoder* audio_decoder_;
  MediaPipelineBackend::VideoDecoder* video_decoder_;
  bool audio_feeding_completed_;
  bool video_feeding_completed_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineDeviceTest);
};

AudioVideoPipelineDeviceTest::AudioVideoPipelineDeviceTest()
    : pause_pattern_(),
      audio_decoder_(nullptr),
      video_decoder_(nullptr),
      audio_feeding_completed_(true),
      video_feeding_completed_(true) {
}

AudioVideoPipelineDeviceTest::~AudioVideoPipelineDeviceTest() {
}

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
  LoadAudioStream(filename);
  bool success = backend_->Initialize(this);
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::ConfigureForVideoOnly(
    const std::string& filename,
    bool raw_h264) {
  Initialize();
  LoadVideoStream(filename, raw_h264);
  bool success = backend_->Initialize(this);
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::ConfigureForFile(
    const std::string& filename) {
  Initialize();
  LoadVideoStream(filename, false /* raw_h264 */);
  LoadAudioStream(filename);
  bool success = backend_->Initialize(this);
  ASSERT_TRUE(success);
}

void AudioVideoPipelineDeviceTest::LoadAudioStream(
    const std::string& filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  DemuxResult demux_result = FFmpegDemuxForTest(file_path, true /* audio */);

  audio_buffers_ = demux_result.frames;
  audio_decoder_ = backend_->CreateAudioDecoder();
  audio_feeding_completed_ = false;

  bool success =
      audio_decoder_->SetConfig(DecoderConfigAdapter::ToCastAudioConfig(
          kPrimary, demux_result.audio_config));
  ASSERT_TRUE(success);

  VLOG(2) << "Got " << audio_buffers_.size() << " audio input frames";

  audio_buffers_.push_back(scoped_refptr<DecoderBufferBase>(
      new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer())));
}

void AudioVideoPipelineDeviceTest::LoadVideoStream(const std::string& filename,
                                                   bool raw_h264) {
  VideoConfig video_config;

  if (raw_h264) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    base::MemoryMappedFile video_stream;
    ASSERT_TRUE(video_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
    video_buffers_ =
        H264SegmenterForTest(video_stream.data(), video_stream.length());

    // TODO(erickung): Either pull data from stream or make caller specify value
    video_config.codec = kCodecH264;
    video_config.profile = kH264Main;
    video_config.additional_config = NULL;
    video_config.is_encrypted = false;
  } else {
    base::FilePath file_path = GetTestDataFilePath(filename);
    DemuxResult demux_result = FFmpegDemuxForTest(file_path,
                                                  /*audio*/ false);
    video_buffers_ = demux_result.frames;
    video_config = DecoderConfigAdapter::ToCastVideoConfig(
        kPrimary, demux_result.video_config);
  }

  video_decoder_ = backend_->CreateVideoDecoder();
  video_feeding_completed_ = false;
  bool success = video_decoder_->SetConfig(video_config);
  ASSERT_TRUE(success);

  VLOG(2) << "Got " << video_buffers_.size() << " video input frames";

  video_buffers_.push_back(scoped_refptr<DecoderBufferBase>(
      new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer())));
}

void AudioVideoPipelineDeviceTest::FeedAudioBuffer() {
  // Possibly feed one frame
  DCHECK(!audio_buffers_.empty());
  if (audio_feeding_completed_)
    return;

  backend_audio_buffer_ = audio_buffers_.front();

  MediaPipelineBackend::BufferStatus status =
      audio_decoder_->PushBuffer(backend_audio_buffer_.get());
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);
  audio_buffers_.pop_front();

  // Feeding is done, just wait for the end of stream callback.
  if (backend_audio_buffer_->end_of_stream() || audio_buffers_.empty()) {
    if (audio_buffers_.empty() && !backend_audio_buffer_->end_of_stream()) {
      LOG(WARNING) << "Stream emptied without feeding EOS frame";
    }

    audio_feeding_completed_ = true;
    return;
  }

  if (status == MediaPipelineBackend::kBufferPending)
    return;

  OnPushBufferComplete(audio_decoder_, MediaPipelineBackend::kBufferSuccess);
}

void AudioVideoPipelineDeviceTest::FeedVideoBuffer() {
  // Possibly feed one frame
  DCHECK(!video_buffers_.empty());
  if (video_feeding_completed_)
    return;

  backend_video_buffer_ = video_buffers_.front();

  MediaPipelineBackend::BufferStatus status =
      video_decoder_->PushBuffer(backend_video_buffer_.get());
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);
  video_buffers_.pop_front();

  // Feeding is done, just wait for the end of stream callback.
  if (backend_video_buffer_->end_of_stream() || video_buffers_.empty()) {
    if (video_buffers_.empty() && !backend_video_buffer_->end_of_stream()) {
      LOG(WARNING) << "Stream emptied without feeding EOS frame";
    }

    video_feeding_completed_ = true;
    return;
  }

  if (status == MediaPipelineBackend::kBufferPending)
    return;

  OnPushBufferComplete(video_decoder_, MediaPipelineBackend::kBufferSuccess);
}

void AudioVideoPipelineDeviceTest::Start() {
  pause_time_ = base::TimeDelta();
  pause_pattern_idx_ = 0;

  if (audio_decoder_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AudioVideoPipelineDeviceTest::FeedAudioBuffer,
                   base::Unretained(this)));
  }
  if (video_decoder_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AudioVideoPipelineDeviceTest::FeedVideoBuffer,
                   base::Unretained(this)));
  }

  backend_->Start(0);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AudioVideoPipelineDeviceTest::MonitorLoop,
                            base::Unretained(this)));
}

void AudioVideoPipelineDeviceTest::OnEndOfStream(
    MediaPipelineBackend::Decoder* decoder) {
  bool success = backend_->Stop();
  ASSERT_TRUE(success);

  if (decoder == audio_decoder_)
    audio_decoder_ = nullptr;
  else if (decoder == video_decoder_)
    video_decoder_ = nullptr;

  if (!audio_decoder_ && !video_decoder_)
    base::MessageLoop::current()->QuitWhenIdle();
}

void AudioVideoPipelineDeviceTest::OnDecoderError(
    MediaPipelineBackend::Decoder* decoder) {
  ASSERT_TRUE(false);
}

void AudioVideoPipelineDeviceTest::OnPushBufferComplete(
    MediaPipelineBackend::Decoder* decoder,
    MediaPipelineBackend::BufferStatus status) {
  EXPECT_NE(status, MediaPipelineBackend::kBufferFailed);

  if (decoder == audio_decoder_) {
    if (audio_feeding_completed_)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AudioVideoPipelineDeviceTest::FeedAudioBuffer,
                   base::Unretained(this)));
  } else if (decoder == video_decoder_) {
    if (video_feeding_completed_)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AudioVideoPipelineDeviceTest::FeedVideoBuffer,
                   base::Unretained(this)));
  }
}

void AudioVideoPipelineDeviceTest::MonitorLoop() {
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
  DCHECK(backend_);
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
