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

static const int kDefaultDurationInMs = 10000;

// Mock of MediaPlayerManager for testing purpose
class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  MockMediaPlayerManager() : num_requests_(0), last_seek_request_id_(0) {}
  virtual ~MockMediaPlayerManager() {};

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
    if (message_loop_.is_running())
      message_loop_.Quit();
  }
  virtual void OnMediaInterrupted(int player_id) OVERRIDE {}
  virtual void OnBufferingUpdate(int player_id, int percentage) OVERRIDE {}
  virtual void OnSeekComplete(int player_id,
                              base::TimeDelta current_time) OVERRIDE {}
  virtual void OnError(int player_id, int error) OVERRIDE {}
  virtual void OnVideoSizeChanged(int player_id, int width,
                                  int height) OVERRIDE {}
  virtual MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE { return NULL; }
  virtual MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE { return NULL; }
  virtual void DestroyAllMediaPlayers() OVERRIDE {}
  virtual void OnReadFromDemuxer(int player_id,
                                 media::DemuxerStream::Type type) OVERRIDE {
    num_requests_++;
    if (message_loop_.is_running())
      message_loop_.Quit();
  }
  virtual void OnMediaSeekRequest(int player_id, base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) OVERRIDE {
    last_seek_request_id_ = seek_request_id;
  }
  virtual void OnMediaConfigRequest(int player_id) OVERRIDE {}
  virtual media::MediaDrmBridge* GetDrmBridge(int media_keys_id) OVERRIDE {
    return NULL;
  }
  virtual void OnProtectedSurfaceRequested(int player_id) OVERRIDE {}
  virtual void OnKeyAdded(int key_id,
                          const std::string& session_id) OVERRIDE {}
  virtual void OnKeyError(int key_id,
                          const std::string& session_id,
                          media::MediaKeys::KeyError error_code,
                          int system_code) OVERRIDE {}
  virtual void OnKeyMessage(int key_id,
                            const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& destination_url) OVERRIDE {}

  int num_requests() const { return num_requests_; }
  unsigned last_seek_request_id() const { return last_seek_request_id_; }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  // The number of request this object sents for decoding data.
  int num_requests_;
  unsigned last_seek_request_id_;
  base::MessageLoop message_loop_;
};

class MediaSourcePlayerTest : public testing::Test {
 public:
  MediaSourcePlayerTest() {
    manager_.reset(new MockMediaPlayerManager());
    player_.reset(new MediaSourcePlayer(0, manager_.get()));
  }
  virtual ~MediaSourcePlayerTest() {}

 protected:
  // Get the decoder job from the MediaSourcePlayer.
  MediaDecoderJob* GetMediaDecoderJob(bool is_audio) {
    if (is_audio) {
      return reinterpret_cast<MediaDecoderJob*>(
          player_->audio_decoder_job_.get());
    }
    return reinterpret_cast<MediaDecoderJob*>(
        player_->video_decoder_job_.get());
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
    player_->DemuxerReady(configs);
    player_->Start();
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

  base::TimeTicks StartTimeTicks() {
    return player_->start_time_ticks_;
  }

  bool IsTypeSupported(const std::vector<uint8>& scheme_uuid,
                       const std::string& security_level,
                       const std::string& container,
                       const std::vector<std::string>& codecs) {
    return MediaSourcePlayer::IsTypeSupported(
        scheme_uuid, security_level, container, codecs);
  }

 protected:
  scoped_ptr<MockMediaPlayerManager> manager_;
  scoped_ptr<MediaSourcePlayer> player_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayerTest);
};

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithValidConfig) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test audio decoder job will be created when codec is successfully started.
  StartAudioDecoderJob();
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithInvalidConfig) {
  if (!MediaCodecBridge::IsAvailable())
    return;

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
  EXPECT_EQ(0, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithValidSurface) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  StartVideoDecoderJob();
  // Video decoder job will not be created until surface is available.
  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());

  player_->SetVideoSurface(surface.Pass());
  EXPECT_EQ(1u, manager_->last_seek_request_id());
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  // The decoder job should be ready now.
  EXPECT_TRUE(NULL != GetMediaDecoderJob(false));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithInvalidSurface) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  StartVideoDecoderJob();
  // Video decoder job will not be created until surface is available.
  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());

  // Release the surface texture.
  surface_texture = NULL;
  player_->SetVideoSurface(surface.Pass());
  EXPECT_EQ(1u, manager_->last_seek_request_id());
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_EQ(NULL, GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, ReadFromDemuxerAfterSeek) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test decoder job will resend a ReadFromDemuxer request after seek.
  StartAudioDecoderJob();
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());

  // Initiate a seek
  player_->SeekTo(base::TimeDelta());
  EXPECT_EQ(1u, manager_->last_seek_request_id());
  // Sending back the seek ACK, this should trigger the player to call
  // OnReadFromDemuxer() again.
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_EQ(2, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, SetSurfaceWhileSeeking) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test SetVideoSurface() will not cause an extra seek while the player is
  // waiting for a seek ACK.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  StartVideoDecoderJob();
  // Player is still waiting for SetVideoSurface(), so no request is sent.
  EXPECT_EQ(0, manager_->num_requests());
  player_->SeekTo(base::TimeDelta());
  EXPECT_EQ(1u, manager_->last_seek_request_id());

  player_->SetVideoSurface(surface.Pass());
  EXPECT_TRUE(NULL == GetMediaDecoderJob(false));
  EXPECT_EQ(1u, manager_->last_seek_request_id());

  // Send the seek ack, player should start requesting data afterwards.
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_TRUE(NULL != GetMediaDecoderJob(false));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartAfterSeekFinish) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test decoder job will not start until all pending seek event is handled.
  DemuxerConfigs configs;
  configs.audio_codec = kCodecVorbis;
  configs.audio_channels = 2;
  configs.audio_sampling_rate = 44100;
  configs.is_audio_encrypted = false;
  configs.duration_ms = kDefaultDurationInMs;
  player_->DemuxerReady(configs);
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, manager_->num_requests());

  // Initiate a seek
  player_->SeekTo(base::TimeDelta());
  EXPECT_EQ(1u, manager_->last_seek_request_id());

  player_->Start();
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, manager_->num_requests());

  // Sending back the seek ACK.
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartImmediatelyAfterPause) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test that if the decoding job is not fully stopped after Pause(),
  // calling Start() will be a noop.
  StartAudioDecoderJob();

  MediaDecoderJob* decoder_job = GetMediaDecoderJob(true);
  EXPECT_TRUE(NULL != decoder_job);
  EXPECT_EQ(1, manager_->num_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());

  // Sending data to player.
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Decoder job will not immediately stop after Pause() since it is
  // running on another thread.
  player_->Pause();
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Nothing happens when calling Start() again.
  player_->Start();
  // Verify that Start() will not destroy and recreate the decoder job.
  EXPECT_EQ(decoder_job, GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  manager_->message_loop()->Run();
  // The decoder job should finish and a new request will be sent.
  EXPECT_EQ(2, manager_->num_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
}

TEST_F(MediaSourcePlayerTest, DecoderJobsCannotStartWithoutAudio) {
  if (!MediaCodecBridge::IsAvailable())
    return;

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
  EXPECT_EQ(0, manager_->num_requests());

  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  player_->SetVideoSurface(surface.Pass());
  EXPECT_EQ(1u, manager_->last_seek_request_id());
  player_->OnSeekRequestAck(manager_->last_seek_request_id());

  MediaDecoderJob* audio_decoder_job = GetMediaDecoderJob(true);
  MediaDecoderJob* video_decoder_job = GetMediaDecoderJob(false);
  EXPECT_EQ(2, manager_->num_requests());
  EXPECT_FALSE(audio_decoder_job->is_decoding());
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending audio data to player, audio decoder should not start.
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForVideo());
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending video data to player, both decoders should start now.
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(audio_decoder_job->is_decoding());
  EXPECT_TRUE(video_decoder_job->is_decoding());
}

TEST_F(MediaSourcePlayerTest, StartTimeTicksResetAfterDecoderUnderruns) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test start time ticks will reset after decoder job underruns.
  StartAudioDecoderJob();
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
  // For the first couple chunks, the decoder job may return
  // DECODE_FORMAT_CHANGED status instead of DECODE_SUCCEEDED status. Decode
  // more frames to guarantee that DECODE_SUCCEEDED will be returned.
  for (int i = 0; i < 4; ++i) {
    player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio(i));
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    manager_->message_loop()->Run();
  }

  // The decoder job should finish and a new request will be sent.
  EXPECT_EQ(5, manager_->num_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
  base::TimeTicks previous = StartTimeTicks();

  // Let the decoder timeout and execute the OnDecoderStarved() callback.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  manager_->message_loop()->RunUntilIdle();

  // Send new data to the decoder. This should reset the start time ticks.
  player_->ReadFromDemuxerAck(CreateEOSAck(true));
  base::TimeTicks current = StartTimeTicks();
  EXPECT_LE(100.0, (current - previous).InMillisecondsF());
}

TEST_F(MediaSourcePlayerTest, NoRequestForDataAfterInputEOS) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test MediaSourcePlayer will not request for new data after input EOS is
  // reached.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      new gfx::SurfaceTexture(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  player_->SetVideoSurface(surface.Pass());
  StartVideoDecoderJob();
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_EQ(1, manager_->num_requests());
  // Send the first input chunk.
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForVideo());
  manager_->message_loop()->Run();
  EXPECT_EQ(2, manager_->num_requests());

  // Send EOS.
  player_->ReadFromDemuxerAck(CreateEOSAck(false));
  manager_->message_loop()->Run();
  // No more request for data should be made.
  EXPECT_EQ(2, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, CanPlayType_Widevine) {
  if (!MediaCodecBridge::IsAvailable() || !MediaDrmBridge::IsAvailable())
    return;

  uint8 kWidevineUUID[] = { 0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
                            0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

  std::vector<uint8> widevine_uuid(kWidevineUUID,
                                   kWidevineUUID + arraysize(kWidevineUUID));

  std::vector<std::string> codec_avc(1, "avc1");
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", "video/mp4", codec_avc));
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L1", "video/mp4", codec_avc));

  std::vector<std::string> codec_vp8(1, "vp8");
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", "video/webm", codec_vp8));
  EXPECT_FALSE(IsTypeSupported(widevine_uuid, "L1", "video/webm", codec_vp8));

  std::vector<std::string> codec_vorbis(1, "vorbis");
  EXPECT_TRUE(IsTypeSupported(widevine_uuid, "L3", "video/webm", codec_vorbis));
  EXPECT_FALSE(
      IsTypeSupported(widevine_uuid, "L1", "video/webm", codec_vorbis));
}

TEST_F(MediaSourcePlayerTest, CanPlayType_InvalidUUID) {
  if (!MediaCodecBridge::IsAvailable() || !MediaDrmBridge::IsAvailable())
    return;

  uint8 kInvalidUUID[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                           0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

  std::vector<uint8> invalid_uuid(kInvalidUUID,
                                  kInvalidUUID + arraysize(kInvalidUUID));

  std::vector<std::string> codec_avc(1, "avc1");
  EXPECT_FALSE(IsTypeSupported(invalid_uuid, "L3", "video/mp4", codec_avc));
  EXPECT_FALSE(IsTypeSupported(invalid_uuid, "L1", "video/mp4", codec_avc));
}

// TODO(xhwang): Are these IsTypeSupported tests device specific?
// TODO(xhwang): Add more IsTypeSupported tests.

}  // namespace media
