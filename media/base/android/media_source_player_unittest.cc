// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_source_player.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/android/surface_texture_bridge.h"

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
    MediaPlayerHostMsg_DemuxerReady_Params params;
    params.audio_codec = kCodecVorbis;
    params.audio_channels = 2;
    params.audio_sampling_rate = 44100;
    params.is_audio_encrypted = false;
    params.duration_ms = kDefaultDurationInMs;
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-extradata");
    params.audio_extra_data = std::vector<uint8>(
        buffer->data(),
        buffer->data() + buffer->data_size());
    Start(params);
  }

  void StartVideoDecoderJob() {
    MediaPlayerHostMsg_DemuxerReady_Params params;
    params.video_codec = kCodecVP8;
    params.video_size = gfx::Size(320, 240);
    params.is_video_encrypted = false;
    params.duration_ms = kDefaultDurationInMs;
    Start(params);
  }

  // Starts decoding the data.
  void Start(const MediaPlayerHostMsg_DemuxerReady_Params& params) {
    player_->DemuxerReady(params);
    player_->Start();
  }

  MediaPlayerHostMsg_ReadFromDemuxerAck_Params
      CreateReadFromDemuxerAckForAudio() {
    MediaPlayerHostMsg_ReadFromDemuxerAck_Params ack_params;
    ack_params.type = DemuxerStream::AUDIO;
    ack_params.access_units.resize(1);
    ack_params.access_units[0].status = DemuxerStream::kOk;
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-packet-0");
    ack_params.access_units[0].data = std::vector<uint8>(
        buffer->data(), buffer->data() + buffer->data_size());
    // Vorbis needs 4 extra bytes padding on Android to decode properly. Check
    // NuMediaExtractor.cpp in Android source code.
    uint8 padding[4] = { 0xff , 0xff , 0xff , 0xff };
    ack_params.access_units[0].data.insert(
        ack_params.access_units[0].data.end(), padding, padding + 4);
    return ack_params;
  }

  MediaPlayerHostMsg_ReadFromDemuxerAck_Params
        CreateReadFromDemuxerAckForVideo() {
    MediaPlayerHostMsg_ReadFromDemuxerAck_Params ack_params;
    ack_params.type = DemuxerStream::VIDEO;
    ack_params.access_units.resize(1);
    ack_params.access_units[0].status = DemuxerStream::kOk;
    scoped_refptr<DecoderBuffer> buffer =
        ReadTestDataFile("vp8-I-frame-320x240");
    ack_params.access_units[0].data = std::vector<uint8>(
        buffer->data(), buffer->data() + buffer->data_size());
    return ack_params;
  }

  base::TimeTicks StartTimeTicks() {
    return player_->start_time_ticks_;
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
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  params.duration_ms = kDefaultDurationInMs;
  uint8 invalid_codec_data[] = { 0x00, 0xff, 0xff, 0xff, 0xff };
  params.audio_extra_data.insert(params.audio_extra_data.begin(),
                                 invalid_codec_data, invalid_codec_data + 4);
  Start(params);
  EXPECT_EQ(NULL, GetMediaDecoderJob(true));
  EXPECT_EQ(0, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithValidSurface) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
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
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
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
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
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
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  params.duration_ms = kDefaultDurationInMs;
  player_->DemuxerReady(params);
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
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio());
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
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vorbis-extradata");
  params.audio_extra_data = std::vector<uint8>(
      buffer->data(),
      buffer->data() + buffer->data_size());
  params.video_codec = kCodecVP8;
  params.video_size = gfx::Size(320, 240);
  params.is_video_encrypted = false;
  params.duration_ms = kDefaultDurationInMs;
  Start(params);
  EXPECT_EQ(0, manager_->num_requests());

  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
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
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio());
  EXPECT_TRUE(audio_decoder_job->is_decoding());
  EXPECT_TRUE(video_decoder_job->is_decoding());
}

// Disabled due to http://crbug.com/266041.
// TODO(xhwang/qinmin): Fix this test and reenable it.
TEST_F(MediaSourcePlayerTest,
       DISABLED_StartTimeTicksResetAfterDecoderUnderruns) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test start time ticks will reset after decoder job underruns.
  StartAudioDecoderJob();
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio());
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  manager_->message_loop()->Run();
  // The decoder job should finish and a new request will be sent.
  EXPECT_EQ(2, manager_->num_requests());
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
  base::TimeTicks previous = StartTimeTicks();

  // Let the decoder timeout and execute the OnDecoderStarved() callback.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  manager_->message_loop()->RunUntilIdle();

  // Send new data to the decoder. This should reset the start time ticks.
  player_->ReadFromDemuxerAck(CreateReadFromDemuxerAckForAudio());
  base::TimeTicks current = StartTimeTicks();
  EXPECT_LE(100.0, (current - previous).InMillisecondsF());
}

TEST_F(MediaSourcePlayerTest, NoRequestForDataAfterInputEOS) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test MediaSourcePlayer will not request for new data after input EOS is
  // reached.
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
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
  MediaPlayerHostMsg_ReadFromDemuxerAck_Params ack_params;
  ack_params.type = DemuxerStream::VIDEO;
  ack_params.access_units.resize(1);
  ack_params.access_units[0].status = DemuxerStream::kOk;
  ack_params.access_units[0].end_of_stream = true;
  player_->ReadFromDemuxerAck(ack_params);
  manager_->message_loop()->Run();
  // No more request for data should be made.
  EXPECT_EQ(2, manager_->num_requests());
}

}  // namespace media
