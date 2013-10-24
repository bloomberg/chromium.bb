// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_source_player.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// Helper macro to skip the test if MediaCodecBridge isn't available.
#define SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE() \
  do { \
    if (!MediaCodecBridge::IsAvailable()) { \
      LOG(INFO) << "Could not run test - not supported on device."; \
      return; \
    } \
  } while (0) \

static const int kDefaultDurationInMs = 10000;

static const char kAudioMp4[] = "audio/mp4";
static const char kVideoMp4[] = "video/mp4";
static const char kAudioWebM[] = "audio/webm";
static const char kVideoWebM[] = "video/webm";

// Mock of MediaPlayerManager for testing purpose
class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  explicit MockMediaPlayerManager(base::MessageLoop* message_loop)
      : message_loop_(message_loop) {}
  virtual ~MockMediaPlayerManager() {}

  // MediaPlayerManager implementation.
  virtual void RequestMediaResources(int player_id) OVERRIDE {}
  virtual void ReleaseMediaResources(int player_id) OVERRIDE {}
  virtual MediaResourceGetter* GetMediaResourceGetter() OVERRIDE {
    return NULL;
  }
  virtual void OnTimeUpdate(int player_id,
                            base::TimeDelta current_time) OVERRIDE {}
  virtual void OnMediaMetadataChanged(
      int player_id, base::TimeDelta duration, int width, int height,
      bool success) OVERRIDE {}
  virtual void OnPlaybackComplete(int player_id) OVERRIDE {
    if (message_loop_->is_running())
      message_loop_->Quit();
  }
  virtual void OnMediaInterrupted(int player_id) OVERRIDE {}
  virtual void OnBufferingUpdate(int player_id, int percentage) OVERRIDE {}
  virtual void OnSeekComplete(int player_id,
                              const base::TimeDelta& current_time) OVERRIDE {}
  virtual void OnError(int player_id, int error) OVERRIDE {}
  virtual void OnVideoSizeChanged(int player_id, int width,
                                  int height) OVERRIDE {}
  virtual MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE { return NULL; }
  virtual MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE { return NULL; }
  virtual void DestroyAllMediaPlayers() OVERRIDE {}
  virtual MediaDrmBridge* GetDrmBridge(int media_keys_id) OVERRIDE {
    return NULL;
  }
  virtual void OnProtectedSurfaceRequested(int player_id) OVERRIDE {}
  virtual void OnKeyAdded(int key_id,
                          const std::string& session_id) OVERRIDE {}
  virtual void OnKeyError(int key_id,
                          const std::string& session_id,
                          MediaKeys::KeyError error_code,
                          int system_code) OVERRIDE {}
  virtual void OnKeyMessage(int key_id,
                            const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& destination_url) OVERRIDE {}

 private:
  base::MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaPlayerManager);
};

class MockDemuxerAndroid : public DemuxerAndroid {
 public:
  explicit MockDemuxerAndroid(base::MessageLoop* message_loop)
      : message_loop_(message_loop),
        num_data_requests_(0),
        num_seek_requests_(0),
        num_config_requests_(0) {}
  virtual ~MockDemuxerAndroid() {}

  virtual void Initialize(DemuxerAndroidClient* client) OVERRIDE {}
  virtual void RequestDemuxerConfigs() OVERRIDE {
    num_config_requests_++;
  }
  virtual void RequestDemuxerData(DemuxerStream::Type type) OVERRIDE {
    num_data_requests_++;
    if (message_loop_->is_running())
      message_loop_->Quit();
  }
  virtual void RequestDemuxerSeek(
      const base::TimeDelta& time_to_seek) OVERRIDE {
    num_seek_requests_++;
  }

  int num_data_requests() const { return num_data_requests_; }
  int num_seek_requests() const { return num_seek_requests_; }
  int num_config_requests() const { return num_config_requests_; }

 private:
  base::MessageLoop* message_loop_;

  // The number of encoded data requests this object has seen.
  int num_data_requests_;

  // The number of seek requests this object has seen.
  int num_seek_requests_;

  // The number of demuxer config requests this object has seen.
  int num_config_requests_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerAndroid);
};

class MediaSourcePlayerTest : public testing::Test {
 public:
  MediaSourcePlayerTest()
      : manager_(&message_loop_),
        demuxer_(new MockDemuxerAndroid(&message_loop_)),
        player_(0, &manager_, scoped_ptr<DemuxerAndroid>(demuxer_)) {}
  virtual ~MediaSourcePlayerTest() {}

 protected:
  // Get the decoder job from the MediaSourcePlayer.
  MediaDecoderJob* GetMediaDecoderJob(bool is_audio) {
    if (is_audio) {
      return reinterpret_cast<MediaDecoderJob*>(
          player_.audio_decoder_job_.get());
    }
    return reinterpret_cast<MediaDecoderJob*>(
        player_.video_decoder_job_.get());
  }

  // Get the per-job prerolling status from the MediaSourcePlayer's job matching
  // |is_audio|. Caller must guard against NPE if the player's job is NULL.
  bool IsPrerolling(bool is_audio) {
    return GetMediaDecoderJob(is_audio)->prerolling();
  }

  // Get the preroll timestamp from the MediaSourcePlayer.
  base::TimeDelta GetPrerollTimestamp() {
    return player_.preroll_timestamp_;
  }

  // Starts an audio decoder job.
  void StartAudioDecoderJob() {
    DemuxerConfigs configs;
    configs.audio_codec = kCodecVorbis;
    configs.audio_channels = 2;
    configs.audio_sampling_rate = 44100;
    configs.is_audio_encrypted = false;
    configs.duration_ms = kDefaultDurationInMs;
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-extradata");
    configs.audio_extra_data = std::vector<uint8>(
        buffer->data(),
        buffer->data() + buffer->data_size());
    Start(configs);
  }

  void StartVideoDecoderJob() {
    DemuxerConfigs configs;
    configs.video_codec = kCodecVP8;
    configs.video_size = gfx::Size(320, 240);
    configs.is_video_encrypted = false;
    configs.duration_ms = kDefaultDurationInMs;
    Start(configs);
  }

  // Starts decoding the data.
  void Start(const DemuxerConfigs& configs) {
    player_.OnDemuxerConfigsAvailable(configs);
    player_.Start();
  }

  DemuxerData CreateReadFromDemuxerAckForAudio(int packet_id) {
    DemuxerData data;
    data.type = DemuxerStream::AUDIO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kOk;
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(
        base::StringPrintf("vorbis-packet-%d", packet_id));
    data.access_units[0].data = std::vector<uint8>(
        buffer->data(), buffer->data() + buffer->data_size());
    // Vorbis needs 4 extra bytes padding on Android to decode properly. Check
    // NuMediaExtractor.cpp in Android source code.
    uint8 padding[4] = { 0xff , 0xff , 0xff , 0xff };
    data.access_units[0].data.insert(
        data.access_units[0].data.end(), padding, padding + 4);
    return data;
  }

  DemuxerData CreateReadFromDemuxerAckForVideo() {
    DemuxerData data;
    data.type = DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kOk;
    scoped_refptr<DecoderBuffer> buffer =
        ReadTestDataFile("vp8-I-frame-320x240");
    data.access_units[0].data = std::vector<uint8>(
        buffer->data(), buffer->data() + buffer->data_size());
    return data;
  }

  DemuxerData CreateEOSAck(bool is_audio) {
    DemuxerData data;
    data.type = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kOk;
    data.access_units[0].end_of_stream = true;
    return data;
  }

  DemuxerData CreateAbortedAck(bool is_audio) {
    DemuxerData data;
    data.type = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kAborted;
    return data;
  }

  // Seek, including simulated receipt of |kAborted| read between SeekTo()
  // and OnDemuxerSeekDone(). Use this helper method only when the player
  // already has created the decoder job.
  void SeekPlayer(bool is_audio, const base::TimeDelta& seek_time) {
    EXPECT_TRUE(GetMediaDecoderJob(is_audio));

    int original_num_seeks = demuxer_->num_seek_requests();
    int original_num_data_requests = demuxer_->num_data_requests();

    // Initiate a seek. Skip the round-trip of requesting seek from renderer.
    // Instead behave as if the renderer has asked us to seek.
    player_.SeekTo(seek_time);

    // Verify that the seek does not occur until previously outstanding data
    // request is satisfied.
    EXPECT_EQ(original_num_seeks, demuxer_->num_seek_requests());

    // Simulate seeking causes the demuxer to abort the outstanding read caused
    // by the seek.
    player_.OnDemuxerDataAvailable(CreateAbortedAck(is_audio));

    // Verify that the seek is requested now that the outstanding read is
    // completed by aborted access unit.
    EXPECT_EQ(original_num_seeks + 1, demuxer_->num_seek_requests());

    // Send back the seek done notification. This should trigger the player to
    // call OnReadFromDemuxer() again.
    EXPECT_EQ(original_num_data_requests, demuxer_->num_data_requests());
    player_.OnDemuxerSeekDone();
    EXPECT_EQ(original_num_data_requests + 1, demuxer_->num_data_requests());

    // No other seek should have been requested.
    EXPECT_EQ(original_num_seeks + 1, demuxer_->num_seek_requests());
  }

  base::TimeTicks StartTimeTicks() {
    return player_.start_time_ticks_;
  }

  bool IsTypeSupported(const std::vector<uint8>& scheme_uuid,
                       const std::string& security_level,
                       const std::string& container,
                       const std::vector<std::string>& codecs) {
    return MediaSourcePlayer::IsTypeSupported(
        scheme_uuid, security_level, container, codecs);
  }

  void CreateAndSetVideoSurface() {
    surface_texture_ = new gfx::SurfaceTexture(0);
    gfx::ScopedJavaSurface surface = gfx::ScopedJavaSurface(
        surface_texture_.get());
    player_.SetVideoSurface(surface.Pass());
  }

 protected:
  base::MessageLoop message_loop_;
  MockMediaPlayerManager manager_;
  MockDemuxerAndroid* demuxer_;  // Owned by |player_|.
  MediaSourcePlayer player_;
  // We need to keep the surface texture while the decoder is actively decoding.
  // Otherwise, it may trigger unexpected crashes on some devices. To switch
  // surfaces, tests need to create their own surface texture without releasing
  // this one if they previously called CreateAndSetVideoSurface().
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayerTest);
};

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithValidConfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test audio decoder job will be created when codec is successfully started.
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithInvalidConfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test audio decoder job will not be created when failed to start the codec.
  DemuxerConfigs configs;
  configs.audio_codec = kCodecVorbis;
  configs.audio_channels = 2;
  configs.audio_sampling_rate = 44100;
  configs.is_audio_encrypted = false;
  configs.duration_ms = kDefaultDurationInMs;
  uint8 invalid_codec_data[] = { 0x00, 0xff, 0xff, 0xff, 0xff };
  configs.audio_extra_data.insert(configs.audio_extra_data.begin(),
                                 invalid_codec_data, invalid_codec_data + 4);
  Start(configs);
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithValidSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test video decoder job will be created when surface is valid.
  StartVideoDecoderJob();
  // Video decoder job will not be created until surface is available.
  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, demuxer_->num_data_requests());

  CreateAndSetVideoSurface();

  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  // The decoder job should be ready now.
  EXPECT_TRUE(GetMediaDecoderJob(false));
  EXPECT_EQ(1, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithInvalidSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  StartVideoDecoderJob();
  // Video decoder job will not be created until surface is available.
  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Release the surface texture.
  surface_texture = NULL;
  player_.SetVideoSurface(surface.Pass());

  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, ReadFromDemuxerAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resend a ReadFromDemuxer request after seek.
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());
  SeekPlayer(true, base::TimeDelta());
  EXPECT_EQ(2, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, SetSurfaceWhileSeeking) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test SetVideoSurface() will not cause an extra seek while the player is
  // waiting for demuxer to indicate seek is done.
  StartVideoDecoderJob();
  // Player is still waiting for SetVideoSurface(), so no request is sent.
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Initiate a seek. Skip the round-trip of requesting seek from renderer.
  // Instead behave as if the renderer has asked us to seek.
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  player_.SeekTo(base::TimeDelta());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  CreateAndSetVideoSurface();
  EXPECT_TRUE(NULL == GetMediaDecoderJob(false));
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  // Reconfirm player has not yet requested data.
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Send the seek done notification. The player should start requesting data.
  player_.OnDemuxerSeekDone();
  EXPECT_TRUE(GetMediaDecoderJob(false));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Reconfirm exactly 1 seek request has been made of demuxer.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, ChangeMultipleSurfaceWhileDecoding) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test MediaSourcePlayer can switch multiple surfaces during decoding.
  CreateAndSetVideoSurface();
  StartVideoDecoderJob();
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo());

  // While the decoder is decoding, change multiple surfaces. Pass an empty
  // surface first.
  gfx::ScopedJavaSurface empty_surface;
  player_.SetVideoSurface(empty_surface.Pass());
  // Pass a new non-empty surface, don't call CreateAndSetVideoSurface() as
  // it will release the old surface_texture_ and might cause unexpected
  // behaviors on some devices.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(1));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  player_.SetVideoSurface(surface.Pass());

  // Wait for the decoder job to finish decoding.
  while (GetMediaDecoderJob(false)->is_decoding())
    message_loop_.RunUntilIdle();
  // A seek should be initiated to request Iframe.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  EXPECT_EQ(1, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, StartAfterSeekFinish) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will not start until all pending seek event is handled.
  DemuxerConfigs configs;
  configs.audio_codec = kCodecVorbis;
  configs.audio_channels = 2;
  configs.audio_sampling_rate = 44100;
  configs.is_audio_encrypted = false;
  configs.duration_ms = kDefaultDurationInMs;
  player_.OnDemuxerConfigsAvailable(configs);
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Initiate a seek. Skip the round-trip of requesting seek from renderer.
  // Instead behave as if the renderer has asked us to seek.
  player_.SeekTo(base::TimeDelta());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  player_.Start();
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Sending back the seek done notification.
  player_.OnDemuxerSeekDone();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Reconfirm exactly 1 seek request has been made of demuxer.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, StartImmediatelyAfterPause) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that if the decoding job is not fully stopped after Pause(),
  // calling Start() will be a noop.
  StartAudioDecoderJob();

  MediaDecoderJob* decoder_job = GetMediaDecoderJob(true);
  EXPECT_TRUE(decoder_job);
  EXPECT_EQ(1, demuxer_->num_data_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());

  // Sending data to player.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Decoder job will not immediately stop after Pause() since it is
  // running on another thread.
  player_.Pause(true);
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Nothing happens when calling Start() again.
  player_.Start();
  // Verify that Start() will not destroy and recreate the decoder job.
  EXPECT_EQ(decoder_job, GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  message_loop_.Run();
  // The decoder job should finish and a new request will be sent.
  EXPECT_EQ(2, demuxer_->num_data_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
}

TEST_F(MediaSourcePlayerTest, DecoderJobsCannotStartWithoutAudio) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that when Start() is called, video decoder jobs will wait for audio
  // decoder job before start decoding the data.
  DemuxerConfigs configs;
  configs.audio_codec = kCodecVorbis;
  configs.audio_channels = 2;
  configs.audio_sampling_rate = 44100;
  configs.is_audio_encrypted = false;
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-extradata");
  configs.audio_extra_data = std::vector<uint8>(
      buffer->data(),
      buffer->data() + buffer->data_size());
  configs.video_codec = kCodecVP8;
  configs.video_size = gfx::Size(320, 240);
  configs.is_video_encrypted = false;
  configs.duration_ms = kDefaultDurationInMs;
  Start(configs);
  EXPECT_EQ(0, demuxer_->num_data_requests());

  CreateAndSetVideoSurface();

  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  MediaDecoderJob* audio_decoder_job = GetMediaDecoderJob(true);
  MediaDecoderJob* video_decoder_job = GetMediaDecoderJob(false);
  EXPECT_EQ(2, demuxer_->num_data_requests());
  EXPECT_FALSE(audio_decoder_job->is_decoding());
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending video data to player, audio decoder should not start.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo());
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending audio data to player, both decoders should start now.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(audio_decoder_job->is_decoding());
  EXPECT_TRUE(video_decoder_job->is_decoding());
}

TEST_F(MediaSourcePlayerTest, StartTimeTicksResetAfterDecoderUnderruns) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test start time ticks will reset after decoder job underruns.
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());
  // For the first couple chunks, the decoder job may return
  // DECODE_FORMAT_CHANGED status instead of DECODE_SUCCEEDED status. Decode
  // more frames to guarantee that DECODE_SUCCEEDED will be returned.
  for (int i = 0; i < 4; ++i) {
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(i));
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    message_loop_.Run();
  }

  // The decoder job should finish and a new request will be sent.
  EXPECT_EQ(5, demuxer_->num_data_requests());
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  base::TimeTicks previous = StartTimeTicks();

  // Let the decoder timeout and execute the OnDecoderStarved() callback.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));

  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  EXPECT_TRUE(StartTimeTicks() != base::TimeTicks());
  message_loop_.RunUntilIdle();

  // Send new data to the decoder so it can finish the currently
  // pending decode.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(3));
  while (GetMediaDecoderJob(true)->is_decoding())
    message_loop_.RunUntilIdle();

  // Verify the start time ticks is cleared at this point because the
  // player is prefetching.
  EXPECT_TRUE(StartTimeTicks() == base::TimeTicks());

  // Send new data to the decoder so it can finish prefetching. This should
  // reset the start time ticks.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(3));
  EXPECT_TRUE(StartTimeTicks() != base::TimeTicks());

  base::TimeTicks current = StartTimeTicks();
  EXPECT_LE(100.0, (current - previous).InMillisecondsF());
}

TEST_F(MediaSourcePlayerTest, NoRequestForDataAfterInputEOS) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test MediaSourcePlayer will not request for new data after input EOS is
  // reached.
  CreateAndSetVideoSurface();
  StartVideoDecoderJob();
  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  EXPECT_EQ(1, demuxer_->num_data_requests());
  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo());
  message_loop_.Run();
  EXPECT_EQ(2, demuxer_->num_data_requests());

  // Send EOS.
  player_.OnDemuxerDataAvailable(CreateEOSAck(false));
  message_loop_.Run();
  // No more request for data should be made.
  EXPECT_EQ(2, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, ReplayAfterInputEOS) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test MediaSourcePlayer can replay after input EOS is
  // reached.
  CreateAndSetVideoSurface();
  StartVideoDecoderJob();

  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  EXPECT_EQ(1, demuxer_->num_data_requests());
  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo());
  message_loop_.Run();
  EXPECT_EQ(2, demuxer_->num_data_requests());

  // Send EOS.
  player_.OnDemuxerDataAvailable(CreateEOSAck(false));
  message_loop_.Run();
  // No more request for data should be made.
  EXPECT_EQ(2, demuxer_->num_data_requests());

  // Initiate a seek. Skip the round-trip of requesting seek from renderer.
  // Instead behave as if the renderer has asked us to seek.
  player_.SeekTo(base::TimeDelta());
  StartVideoDecoderJob();
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  player_.OnDemuxerSeekDone();
  // Seek/Play after EOS should request more data.
  EXPECT_EQ(3, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, NoRequestForDataAfterAbort) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the decoder will request new data after receiving an aborted
  // access unit.
  StartAudioDecoderJob();
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Send an aborted access unit.
  player_.OnDemuxerDataAvailable(CreateAbortedAck(true));

  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  // Wait for the decoder job to finish decoding.
  while (GetMediaDecoderJob(true)->is_decoding())
    message_loop_.RunUntilIdle();

  // No request will be sent for new data.
  EXPECT_EQ(1, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, DemuxerDataArrivesAfterRelease) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the decoder should not crash if demuxer data arrives after
  // Release().
  StartAudioDecoderJob();
  EXPECT_TRUE(player_.IsPlaying());
  EXPECT_EQ(1, demuxer_->num_data_requests());
  EXPECT_TRUE(GetMediaDecoderJob(true));

  player_.Release();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));

  // The decoder job should have been released.
  EXPECT_FALSE(player_.IsPlaying());
  EXPECT_EQ(1, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, PrerollAudioAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will preroll the media to the seek position.
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  SeekPlayer(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // Send some data before the seek position.
  for (int i = 1; i < 4; ++i) {
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(i));
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    message_loop_.Run();
  }
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_TRUE(IsPrerolling(true));

  // Send data after the seek position.
  DemuxerData data = CreateReadFromDemuxerAckForAudio(3);
  data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(100);
  player_.OnDemuxerDataAvailable(data);
  message_loop_.Run();
  EXPECT_LT(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));
}

TEST_F(MediaSourcePlayerTest, PrerollVideoAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will preroll the media to the seek position.
  CreateAndSetVideoSurface();
  StartVideoDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(false));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  SeekPlayer(false, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(false));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // Send some data before the seek position.
  DemuxerData data;
  for (int i = 1; i < 4; ++i) {
    data = CreateReadFromDemuxerAckForVideo();
    data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(i * 30);
    player_.OnDemuxerDataAvailable(data);
    EXPECT_TRUE(GetMediaDecoderJob(false)->is_decoding());
    message_loop_.Run();
  }
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_TRUE(IsPrerolling(false));

  // Send data at the seek position.
  data = CreateReadFromDemuxerAckForVideo();
  data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(100);
  player_.OnDemuxerDataAvailable(data);
  message_loop_.Run();

  // TODO(wolenetz/qinmin): Player's maintenance of current time for video-only
  // streams depends on decoder output, which may be initially inaccurate, and
  // encoded video test data may also need updating. Verify at least that AU
  // timestamp-based preroll logic has determined video preroll has completed.
  EXPECT_FALSE(IsPrerolling(false));
}

TEST_F(MediaSourcePlayerTest, SeekingAfterCompletingPrerollRestartsPreroll) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will begin prerolling upon seek, when it was not
  // prerolling prior to the seek.
  StartAudioDecoderJob();
  MediaDecoderJob* decoder_job = GetMediaDecoderJob(true);
  EXPECT_TRUE(decoder_job);
  EXPECT_EQ(1, demuxer_->num_data_requests());
  EXPECT_TRUE(IsPrerolling(true));

  // Complete the initial preroll by feeding data to the decoder.
  for (int i = 0; i < 4; ++i) {
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(i));
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    message_loop_.Run();
  }
  EXPECT_LT(0.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));

  SeekPlayer(true, base::TimeDelta::FromMilliseconds(500));

  // Prerolling should have begun again.
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(500.0, GetPrerollTimestamp().InMillisecondsF());

  // Send data at and after the seek position. Prerolling should complete.
  for (int i = 0; i < 4; ++i) {
    DemuxerData data = CreateReadFromDemuxerAckForAudio(i);
    data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(
        500 + 30 * (i - 1));
    player_.OnDemuxerDataAvailable(data);
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    message_loop_.Run();
  }
  EXPECT_LT(500.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));

  // Throughout this test, we should have not re-created the decoder job, so
  // IsPrerolling() transition from false to true was not due to constructor
  // initialization. It was due to BeginPrerolling().
  EXPECT_EQ(decoder_job, GetMediaDecoderJob(true));
}

TEST_F(MediaSourcePlayerTest, PrerollContinuesAcrossReleaseAndStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resume media prerolling if interrupted by Release()
  // and Start().
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  SeekPlayer(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // Send some data before the seek position.
  // Test uses 'large' number of iterations because decoder job may not get
  // MEDIA_CODEC_OK output status until after a few dequeue output attempts.
  // This allows decoder status to stabilize prior to AU timestamp reaching
  // the preroll target.
  DemuxerData data;
  for (int i = 0; i < 10; ++i) {
    data = CreateReadFromDemuxerAckForAudio(3);
    data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(i * 10);
    if (i == 1) {
      // While still prerolling, Release() and Start() the player.
      // TODO(qinmin): Simulation of multiple in-flight data requests (one from
      // before Release(), one from after Start()) is not included here, and
      // neither is any data enqueued for later decode if it arrives after
      // Release() and before Start(). See http://crbug.com/306314. Assumption
      // for this test, to prevent flakiness until the bug is fixed, is the
      // first request's data arrives before Start(). Though that data is not
      // seen by decoder, this assumption allows preroll continuation
      // verification and prevents multiple in-flight data requests.
      player_.Release();
      player_.OnDemuxerDataAvailable(data);
      message_loop_.RunUntilIdle();
      EXPECT_EQ(NULL, GetMediaDecoderJob(true));
      StartAudioDecoderJob();
      EXPECT_TRUE(GetMediaDecoderJob(true));
    } else {
      player_.OnDemuxerDataAvailable(data);
      EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
      message_loop_.Run();
    }
    EXPECT_TRUE(IsPrerolling(true));
  }
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_TRUE(IsPrerolling(true));

  // Send data after the seek position.
  data = CreateReadFromDemuxerAckForAudio(3);
  data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(100);
  player_.OnDemuxerDataAvailable(data);
  message_loop_.Run();
  EXPECT_LT(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));
}

TEST_F(MediaSourcePlayerTest, PrerollContinuesAcrossConfigChange) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resume media prerolling if interrupted by
  // |kConfigChanged| and OnDemuxerConfigsAvailable().
  StartAudioDecoderJob();
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  SeekPlayer(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // In response to data request, simulate that demuxer signals config change by
  // sending an AU with |kConfigChanged|. Player should prepare to reconfigure
  // the audio decoder job, and should request new demuxer configs.
  DemuxerData data;
  data.type = DemuxerStream::AUDIO;
  data.access_units.resize(1);
  data.access_units[0].status = DemuxerStream::kConfigChanged;
  EXPECT_EQ(0, demuxer_->num_config_requests());
  player_.OnDemuxerDataAvailable(data);
  EXPECT_EQ(1, demuxer_->num_config_requests());

  // Simulate arrival of new configs.
  DemuxerConfigs configs;
  configs.audio_codec = kCodecVorbis;
  configs.audio_channels = 2;
  configs.audio_sampling_rate = 44100;
  configs.is_audio_encrypted = false;
  configs.duration_ms = kDefaultDurationInMs;
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-extradata");
  configs.audio_extra_data = std::vector<uint8>(
      buffer->data(),
      buffer->data() + buffer->data_size());
  player_.OnDemuxerConfigsAvailable(configs);

  // Send some data before the seek position.
  for (int i = 1; i < 4; ++i) {
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(i));
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    message_loop_.Run();
  }
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_TRUE(IsPrerolling(true));

  // Send data after the seek position.
  data = CreateReadFromDemuxerAckForAudio(3);
  data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(100);
  player_.OnDemuxerDataAvailable(data);
  message_loop_.Run();
  EXPECT_LT(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));
}

// TODO(xhwang): Enable this test when the test devices are updated.
TEST_F(MediaSourcePlayerTest, DISABLED_IsTypeSupported_Widevine) {
  if (!MediaCodecBridge::IsAvailable() || !MediaDrmBridge::IsAvailable()) {
    LOG(INFO) << "Could not run test - not supported on device.";
    return;
  }

  uint8 kWidevineUUID[] = { 0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
                            0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

  std::vector<uint8> widevine_uuid(kWidevineUUID,
                                   kWidevineUUID + arraysize(kWidevineUUID));

  // We test "L3" fully. But for "L1" we don't check the result as it depend on
  // whether the test device supports "L1" decoding.

  std::vector<std::string> codec_avc(1, "avc1");
  std::vector<std::string> codec_aac(1, "mp4a");
  std::vector<std::string> codec_avc_aac(1, "avc1");
  codec_avc_aac.push_back("mp4a");

  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", kVideoMp4, codec_avc));
  IsTypeSupported(widevine_uuid, "L1", kVideoMp4, codec_avc);

  // TODO(xhwang): L1/L3 doesn't apply to audio, so the result is messy.
  // Clean this up after we have a solution to specifying decoding mode.
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", kAudioMp4, codec_aac));
  IsTypeSupported(widevine_uuid, "L1", kAudioMp4, codec_aac);

  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", kVideoMp4, codec_avc_aac));
  IsTypeSupported(widevine_uuid, "L1", kVideoMp4, codec_avc_aac);

  std::vector<std::string> codec_vp8(1, "vp8");
  std::vector<std::string> codec_vorbis(1, "vorbis");
  std::vector<std::string> codec_vp8_vorbis(1, "vp8");
  codec_vp8_vorbis.push_back("vorbis");

  // TODO(xhwang): WebM is actually not supported but currently
  // MediaDrmBridge.isCryptoSchemeSupported() doesn't check the container type.
  // Fix isCryptoSchemeSupported() and update this test as necessary.
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", kVideoWebM, codec_vp8));
  IsTypeSupported(widevine_uuid, "L1", kVideoWebM, codec_vp8);

  // TODO(xhwang): L1/L3 doesn't apply to audio, so the result is messy.
  // Clean this up after we have a solution to specifying decoding mode.
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", kAudioWebM, codec_vorbis));
  IsTypeSupported(widevine_uuid, "L1", kAudioWebM, codec_vorbis);

  EXPECT_TRUE(
      IsTypeSupported(widevine_uuid, "L3", kVideoWebM, codec_vp8_vorbis));
  IsTypeSupported(widevine_uuid, "L1", kVideoWebM, codec_vp8_vorbis);
}

TEST_F(MediaSourcePlayerTest, IsTypeSupported_InvalidUUID) {
  if (!MediaCodecBridge::IsAvailable() || !MediaDrmBridge::IsAvailable()) {
    LOG(INFO) << "Could not run test - not supported on device.";
    return;
  }

  uint8 kInvalidUUID[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                           0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

  std::vector<uint8> invalid_uuid(kInvalidUUID,
                                  kInvalidUUID + arraysize(kInvalidUUID));

  std::vector<std::string> codec_avc(1, "avc1");
  EXPECT_FALSE(IsTypeSupported(invalid_uuid, "L3", kVideoMp4, codec_avc));
  EXPECT_FALSE(IsTypeSupported(invalid_uuid, "L1", kVideoMp4, codec_avc));
}

// TODO(xhwang): Are these IsTypeSupported tests device specific?
// TODO(xhwang): Add more IsTypeSupported tests.
// TODO(wolenetz): Add tests around second SetVideoSurface, once fix to reach
// next I-frame is correct.

}  // namespace media
