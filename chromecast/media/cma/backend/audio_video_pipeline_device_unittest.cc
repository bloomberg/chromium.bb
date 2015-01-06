// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/base/decrypt_context.h"
#include "chromecast/media/cma/backend/audio_pipeline_device.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device_params.h"
#include "chromecast/media/cma/backend/video_pipeline_device.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/test/frame_segmenter_for_test.h"
#include "chromecast/media/cma/test/media_component_device_feeder_for_test.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

typedef ScopedVector<MediaComponentDeviceFeederForTest>::iterator
    ComponentDeviceIterator;

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

  void ConfigureForFile(std::string filename);
  void ConfigureForAudioOnly(std::string filename);
  void ConfigureForVideoOnly(std::string filename, bool raw_h264);

  // Pattern loops, waiting >= pattern[i].delay against media clock between
  // pauses, then pausing for >= pattern[i].length against MessageLoop
  // A pause with delay <0 signals to stop sequence and do not loop
  void SetPausePattern(const std::vector<PauseInfo> pattern);

  // Adds a pause to the end of pause pattern
  void AddPause(base::TimeDelta delay, base::TimeDelta length);

  void Start();

 private:
  void Initialize();

  void LoadAudioStream(std::string filename);
  void LoadVideoStream(std::string filename, bool raw_h264);

  void MonitorLoop();

  void OnPauseCompleted();

  void OnEos(MediaComponentDeviceFeederForTest* device_feeder);

  scoped_ptr<MediaPipelineDevice> media_pipeline_device_;
  MediaClockDevice* media_clock_device_;

  // Devices to feed
  ScopedVector<MediaComponentDeviceFeederForTest>
      component_device_feeders_;

  // Current media time.
  base::TimeDelta pause_time_;

  // Pause settings
  std::vector<PauseInfo> pause_pattern_;
  int pause_pattern_idx_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineDeviceTest);
};

AudioVideoPipelineDeviceTest::AudioVideoPipelineDeviceTest()
    : pause_pattern_() {
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

void AudioVideoPipelineDeviceTest::ConfigureForAudioOnly(std::string filename) {
  Initialize();
  LoadAudioStream(filename);
}

void AudioVideoPipelineDeviceTest::ConfigureForVideoOnly(std::string filename,
                                                         bool raw_h264) {
  Initialize();
  LoadVideoStream(filename, raw_h264);
}

void AudioVideoPipelineDeviceTest::ConfigureForFile(std::string filename) {
  Initialize();
  LoadVideoStream(filename, false /* raw_h264 */);
  LoadAudioStream(filename);
}

void AudioVideoPipelineDeviceTest::LoadAudioStream(std::string filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  DemuxResult demux_result = FFmpegDemuxForTest(file_path, true /* audio */);
  BufferList frames = demux_result.frames;

  AudioPipelineDevice* audio_pipeline_device =
      media_pipeline_device_->GetAudioPipelineDevice();

  bool success = audio_pipeline_device->SetConfig(demux_result.audio_config);
  ASSERT_TRUE(success);

  VLOG(2) << "Got " << frames.size() << " audio input frames";

  frames.push_back(
      scoped_refptr<DecoderBufferBase>(
          new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer())));

  MediaComponentDeviceFeederForTest* device_feeder =
      new MediaComponentDeviceFeederForTest(audio_pipeline_device, frames);
  device_feeder->Initialize(base::Bind(&AudioVideoPipelineDeviceTest::OnEos,
                                       base::Unretained(this),
                                       device_feeder));
  component_device_feeders_.push_back(device_feeder);
}

void AudioVideoPipelineDeviceTest::LoadVideoStream(std::string filename,
                                                   bool raw_h264) {
  BufferList frames;
  ::media::VideoDecoderConfig video_config;

  if (raw_h264) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    base::MemoryMappedFile video_stream;
    ASSERT_TRUE(video_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
    frames = H264SegmenterForTest(video_stream.data(), video_stream.length());

    // Use arbitraty sizes.
    gfx::Size coded_size(320, 240);
    gfx::Rect visible_rect(0, 0, 320, 240);
    gfx::Size natural_size(320, 240);

    // TODO(kjoswiak): Either pull data from stream or make caller specify value
    video_config = ::media::VideoDecoderConfig(
        ::media::kCodecH264,
        ::media::H264PROFILE_MAIN,
        ::media::VideoFrame::I420,
        coded_size,
        visible_rect,
        natural_size,
        NULL, 0, false);
  } else {
    base::FilePath file_path = GetTestDataFilePath(filename);
    DemuxResult demux_result = FFmpegDemuxForTest(file_path,
                                                  /*audio*/ false);
    frames = demux_result.frames;
    video_config = demux_result.video_config;
  }

  VideoPipelineDevice* video_pipeline_device =
      media_pipeline_device_->GetVideoPipelineDevice();

  // Set configuration.
  bool success = video_pipeline_device->SetConfig(video_config);
  ASSERT_TRUE(success);

  VLOG(2) << "Got " << frames.size() << " video input frames";

  frames.push_back(
      scoped_refptr<DecoderBufferBase>(new DecoderBufferAdapter(
          ::media::DecoderBuffer::CreateEOSBuffer())));

  MediaComponentDeviceFeederForTest* device_feeder =
      new MediaComponentDeviceFeederForTest(video_pipeline_device, frames);
  device_feeder->Initialize(base::Bind(&AudioVideoPipelineDeviceTest::OnEos,
                                       base::Unretained(this),
                                       device_feeder));
  component_device_feeders_.push_back(device_feeder);
}

void AudioVideoPipelineDeviceTest::Start() {
  pause_time_ = base::TimeDelta();
  pause_pattern_idx_ = 0;

  for (int i = 0; i < component_device_feeders_.size(); i++) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&MediaComponentDeviceFeederForTest::Feed,
                   base::Unretained(component_device_feeders_[i])));
  }

  media_clock_device_->SetState(MediaClockDevice::kStateRunning);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&AudioVideoPipelineDeviceTest::MonitorLoop,
                 base::Unretained(this)));
}

void AudioVideoPipelineDeviceTest::MonitorLoop() {
  base::TimeDelta media_time = media_clock_device_->GetTime();

  if (!pause_pattern_.empty() &&
      pause_pattern_[pause_pattern_idx_].delay >= base::TimeDelta() &&
      media_time >= pause_time_ + pause_pattern_[pause_pattern_idx_].delay) {
    // Do Pause
    media_clock_device_->SetRate(0.0);
    pause_time_ = media_clock_device_->GetTime();

    VLOG(2) << "Pausing at " << pause_time_.InMilliseconds() << "ms for " <<
        pause_pattern_[pause_pattern_idx_].length.InMilliseconds() << "ms";

    // Wait for pause finish
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioVideoPipelineDeviceTest::OnPauseCompleted,
                   base::Unretained(this)),
        pause_pattern_[pause_pattern_idx_].length);
    return;
  }

  // Check state again in a little while
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioVideoPipelineDeviceTest::MonitorLoop,
                 base::Unretained(this)),
      kMonitorLoopDelay);
}

void AudioVideoPipelineDeviceTest::OnPauseCompleted() {
  // Make sure the media time didn't move during that time.
  base::TimeDelta media_time = media_clock_device_->GetTime();

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
  media_clock_device_->SetRate(1.0);

  MonitorLoop();
}

void AudioVideoPipelineDeviceTest::OnEos(
    MediaComponentDeviceFeederForTest* device_feeder) {
  for (ComponentDeviceIterator it = component_device_feeders_.begin();
       it != component_device_feeders_.end();
       ++it) {
    if (*it == device_feeder) {
      component_device_feeders_.erase(it);
      break;
    }
  }

  // Check if all streams finished
  if (component_device_feeders_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

void AudioVideoPipelineDeviceTest::Initialize() {
  // Create the media device.
  MediaPipelineDeviceParams params;
  media_pipeline_device_.reset(CreateMediaPipelineDevice(params).release());
  media_clock_device_ = media_pipeline_device_->GetMediaClockDevice();

  // Clock initialization and configuration.
  bool success =
      media_clock_device_->SetState(MediaClockDevice::kStateIdle);
  ASSERT_TRUE(success);
  success = media_clock_device_->ResetTimeline(base::TimeDelta());
  ASSERT_TRUE(success);
  media_clock_device_->SetRate(1.0);
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
