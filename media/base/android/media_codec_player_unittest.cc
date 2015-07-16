// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_player.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/test_data_factory.h"
#include "media/base/android/test_statistics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// Helper macro to skip the test if MediaCodecBridge isn't available.
#define SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE()        \
  do {                                                            \
    if (!MediaCodecBridge::IsAvailable()) {                       \
      VLOG(0) << "Could not run test - not supported on device."; \
      return;                                                     \
    }                                                             \
  } while (0)

#define RUN_ON_MEDIA_THREAD(CLASS, METHOD, ...)                               \
  do {                                                                        \
    if (!GetMediaTaskRunner()->BelongsToCurrentThread()) {                    \
      GetMediaTaskRunner()->PostTask(                                         \
          FROM_HERE,                                                          \
          base::Bind(&CLASS::METHOD, base::Unretained(this), ##__VA_ARGS__)); \
      return;                                                                 \
    }                                                                         \
  } while (0)

namespace {

const base::TimeDelta kDefaultTimeout = base::TimeDelta::FromMilliseconds(200);
const base::TimeDelta kAudioFramePeriod =
    base::TimeDelta::FromSecondsD(1024.0 / 44100);  // 1024 samples @ 44100 Hz
const base::TimeDelta kVideoFramePeriod = base::TimeDelta::FromMilliseconds(20);

// Mock of MediaPlayerManager for testing purpose.

class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  MockMediaPlayerManager()
      : playback_completed_(false), weak_ptr_factory_(this) {}
  ~MockMediaPlayerManager() override {}

  MediaResourceGetter* GetMediaResourceGetter() override { return nullptr; }
  MediaUrlInterceptor* GetMediaUrlInterceptor() override { return nullptr; }

  void OnTimeUpdate(int player_id,
                    base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override {
    pts_stat_.AddValue(current_timestamp);
  }

  void OnMediaMetadataChanged(int player_id,
                              base::TimeDelta duration,
                              int width,
                              int height,
                              bool success) override {
    media_metadata_.duration = duration;
    media_metadata_.width = width;
    media_metadata_.height = height;
    media_metadata_.modified = true;
  }

  void OnPlaybackComplete(int player_id) override {
    playback_completed_ = true;
  }

  void OnMediaInterrupted(int player_id) override {}
  void OnBufferingUpdate(int player_id, int percentage) override {}
  void OnSeekComplete(int player_id,
                      const base::TimeDelta& current_time) override {}
  void OnError(int player_id, int error) override {}
  void OnVideoSizeChanged(int player_id, int width, int height) override {}
  void OnAudibleStateChanged(int player_id, bool is_audible_now) override {}
  void OnWaitingForDecryptionKey(int player_id) override {}
  MediaPlayerAndroid* GetFullscreenPlayer() override { return nullptr; }
  MediaPlayerAndroid* GetPlayer(int player_id) override { return nullptr; }
  bool RequestPlay(int player_id) override { return true; }

  void OnMediaResourcesRequested(int player_id) {}

  base::WeakPtr<MockMediaPlayerManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Conditions to wait for.
  bool IsMetadataChanged() const { return media_metadata_.modified; }
  bool IsPlaybackCompleted() const { return playback_completed_; }

  struct MediaMetadata {
    base::TimeDelta duration;
    int width;
    int height;
    bool modified;
    MediaMetadata() : width(0), height(0), modified(false) {}
  };
  MediaMetadata media_metadata_;

  Minimax<base::TimeDelta> pts_stat_;

 private:
  bool playback_completed_;

  base::WeakPtrFactory<MockMediaPlayerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaPlayerManager);
};

// Helper method that creates demuxer configuration.

DemuxerConfigs CreateAudioVideoConfigs(const base::TimeDelta& duration,
                                       const gfx::Size& video_size) {
  DemuxerConfigs configs =
      TestDataFactory::CreateAudioConfigs(kCodecAAC, duration);
  configs.video_codec = kCodecVP8;
  configs.video_size = video_size;
  configs.is_video_encrypted = false;
  return configs;
}

DemuxerConfigs CreateAudioVideoConfigs(const TestDataFactory* audio,
                                       const TestDataFactory* video) {
  DemuxerConfigs result = audio->GetConfigs();
  DemuxerConfigs vconf = video->GetConfigs();

  result.video_codec = vconf.video_codec;
  result.video_size = vconf.video_size;
  result.is_video_encrypted = vconf.is_video_encrypted;
  return result;
}

// AudioFactory creates data chunks that simulate audio stream from demuxer.

class AudioFactory : public TestDataFactory {
 public:
  AudioFactory(base::TimeDelta duration)
      : TestDataFactory("aac-44100-packet-%d", duration, kAudioFramePeriod) {}

  DemuxerConfigs GetConfigs() const override {
    return TestDataFactory::CreateAudioConfigs(kCodecAAC, duration_);
  }

 protected:
  void ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) override {
    unit->is_key_frame = true;
  }
};

// VideoFactory creates a video stream from demuxer.

class VideoFactory : public TestDataFactory {
 public:
  VideoFactory(base::TimeDelta duration)
      : TestDataFactory("h264-320x180-frame-%d", duration, kVideoFramePeriod) {}

  DemuxerConfigs GetConfigs() const override {
    return TestDataFactory::CreateVideoConfigs(kCodecH264, duration_,
                                               gfx::Size(320, 180));
  }

 protected:
  void ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) override {
    // The frames are taken from High profile and some are B-frames.
    // The first 4 frames appear in the file in the following order:
    //
    // Frames:             I P B P
    // Decoding order:     0 1 2 3
    // Presentation order: 0 2 1 4(3)
    //
    // I keep the last PTS to be 3 for simplicity.

    // Swap pts for second and third frames. Make first frame a key frame.
    switch (index_in_chunk) {
      case 0:  // first frame
        unit->is_key_frame = true;
        break;
      case 1:  // second frame
        unit->timestamp += frame_period_;
        break;
      case 2:  // third frame
        unit->timestamp -= frame_period_;
        break;
      case 3:  // fourth frame, do not modify
        break;
      default:
        NOTREACHED();
        break;
    }
  }
};

// Mock of DemuxerAndroid for testing purpose.

class MockDemuxerAndroid : public DemuxerAndroid {
 public:
  MockDemuxerAndroid() : client_(nullptr) {}
  ~MockDemuxerAndroid() override {}

  // DemuxerAndroid implementation
  void Initialize(DemuxerAndroidClient* client) override;
  void RequestDemuxerData(DemuxerStream::Type type) override;
  void RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                          bool is_browser_seek) override {}

  // Sets the audio data factory.
  void SetAudioFactory(scoped_ptr<TestDataFactory> factory) {
    audio_factory_ = factory.Pass();
  }

  // Sets the video data factory.
  void SetVideoFactory(scoped_ptr<TestDataFactory> factory) {
    video_factory_ = factory.Pass();
  }

  // Post DemuxerConfigs to the client (i.e. the player) on correct thread.
  void PostConfigs(const DemuxerConfigs& configs);

  // Post DemuxerConfigs derived from data factories that has been set.
  void PostInternalConfigs();

  // Conditions to wait for.
  bool IsInitialized() const { return client_; }
  bool HasPendingConfigs() const { return pending_configs_; }

 private:
  DemuxerAndroidClient* client_;
  scoped_ptr<DemuxerConfigs> pending_configs_;
  scoped_ptr<TestDataFactory> audio_factory_;
  scoped_ptr<TestDataFactory> video_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerAndroid);
};

void MockDemuxerAndroid::Initialize(DemuxerAndroidClient* client) {
  DVLOG(1) << "MockDemuxerAndroid::" << __FUNCTION__;
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  client_ = client;
  if (pending_configs_)
    client_->OnDemuxerConfigsAvailable(*pending_configs_);
}

void MockDemuxerAndroid::RequestDemuxerData(DemuxerStream::Type type) {
  DemuxerData chunk;
  base::TimeDelta delay;

  bool created = false;
  if (type == DemuxerStream::AUDIO && audio_factory_)
    created = audio_factory_->CreateChunk(&chunk, &delay);
  else if (type == DemuxerStream::VIDEO && video_factory_)
    created = video_factory_->CreateChunk(&chunk, &delay);

  if (!created)
    return;

  chunk.type = type;

  // Post to Media thread.
  DCHECK(client_);
  GetMediaTaskRunner()->PostDelayedTask(
      FROM_HERE, base::Bind(&DemuxerAndroidClient::OnDemuxerDataAvailable,
                            base::Unretained(client_), chunk),
      delay);
}

void MockDemuxerAndroid::PostConfigs(const DemuxerConfigs& configs) {
  RUN_ON_MEDIA_THREAD(MockDemuxerAndroid, PostConfigs, configs);

  DVLOG(1) << "MockDemuxerAndroid::" << __FUNCTION__;

  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  if (client_)
    client_->OnDemuxerConfigsAvailable(configs);
  else
    pending_configs_ = scoped_ptr<DemuxerConfigs>(new DemuxerConfigs(configs));
}

void MockDemuxerAndroid::PostInternalConfigs() {
  ASSERT_TRUE(audio_factory_ || video_factory_);

  if (audio_factory_ && video_factory_) {
    PostConfigs(
        CreateAudioVideoConfigs(audio_factory_.get(), video_factory_.get()));
  } else if (audio_factory_) {
    PostConfigs(audio_factory_->GetConfigs());
  } else if (video_factory_) {
    PostConfigs(video_factory_->GetConfigs());
  }
}

}  // namespace (anonymous)

// The test fixture for MediaCodecPlayer

class MediaCodecPlayerTest : public testing::Test {
 public:
  MediaCodecPlayerTest();
  ~MediaCodecPlayerTest() override;

 protected:
  typedef base::Callback<bool()> Predicate;

  void CreatePlayer();
  void SetVideoSurface();

  // Waits for condition to become true or for timeout to expire.
  // Returns true if the condition becomes true.
  bool WaitForCondition(const Predicate& condition,
                        const base::TimeDelta& timeout = kDefaultTimeout);

  base::MessageLoop message_loop_;
  MockMediaPlayerManager manager_;
  MockDemuxerAndroid* demuxer_;  // owned by player_
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;
  MediaCodecPlayer* player_;     // raw pointer due to DeleteOnCorrectThread()

 private:
  bool is_timeout_expired() const { return is_timeout_expired_; }
  void SetTimeoutExpired(bool value) { is_timeout_expired_ = value; }

  bool is_timeout_expired_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecPlayerTest);
};

MediaCodecPlayerTest::MediaCodecPlayerTest()
    : demuxer_(new MockDemuxerAndroid()), player_(nullptr) {
}

MediaCodecPlayerTest::~MediaCodecPlayerTest() {
  if (player_)
    player_->DeleteOnCorrectThread();
}

void MediaCodecPlayerTest::CreatePlayer() {
  DCHECK(demuxer_);
  player_ = new MediaCodecPlayer(
      0,  // player_id
      manager_.GetWeakPtr(),
      base::Bind(&MockMediaPlayerManager::OnMediaResourcesRequested,
                 base::Unretained(&manager_)),
      scoped_ptr<MockDemuxerAndroid>(demuxer_), GURL());

  DCHECK(player_);
}

void MediaCodecPlayerTest::SetVideoSurface() {
  surface_texture_ = gfx::SurfaceTexture::Create(0);
  gfx::ScopedJavaSurface surface(surface_texture_.get());

  ASSERT_NE(nullptr, player_);
  player_->SetVideoSurface(surface.Pass());
}

bool MediaCodecPlayerTest::WaitForCondition(const Predicate& condition,
                                            const base::TimeDelta& timeout) {
  // Let the message_loop_ process events.
  // We start the timer and RunUntilIdle() until it signals.

  SetTimeoutExpired(false);

  base::Timer timer(false, false);
  timer.Start(FROM_HERE, timeout,
              base::Bind(&MediaCodecPlayerTest::SetTimeoutExpired,
                         base::Unretained(this), true));

  do {
    if (condition.Run()) {
      timer.Stop();
      return true;
    }
    message_loop_.RunUntilIdle();
  } while (!is_timeout_expired());

  DCHECK(!timer.IsRunning());
  return false;
}

TEST_F(MediaCodecPlayerTest, SetAudioConfigsBeforePlayerCreation) {
  // Post configuration when there is no player yet.
  EXPECT_EQ(nullptr, player_);

  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  demuxer_->PostConfigs(
      TestDataFactory::CreateAudioConfigs(kCodecAAC, duration));

  // Wait until the configuration gets to the media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockDemuxerAndroid::HasPendingConfigs, base::Unretained(demuxer_))));

  // Then create the player.
  CreatePlayer();

  // Configuration should propagate through the player and to the manager.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsMetadataChanged,
                                  base::Unretained(&manager_))));

  EXPECT_EQ(duration, manager_.media_metadata_.duration);
  EXPECT_EQ(0, manager_.media_metadata_.width);
  EXPECT_EQ(0, manager_.media_metadata_.height);
}

TEST_F(MediaCodecPlayerTest, SetAudioConfigsAfterPlayerCreation) {
  CreatePlayer();

  // Wait till the player is initialized on media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(&MockDemuxerAndroid::IsInitialized,
                                          base::Unretained(demuxer_))));

  // Post configuration after the player has been initialized.
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);
  demuxer_->PostConfigs(
      TestDataFactory::CreateAudioConfigs(kCodecAAC, duration));

  // Configuration should propagate through the player and to the manager.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsMetadataChanged,
                                  base::Unretained(&manager_))));

  EXPECT_EQ(duration, manager_.media_metadata_.duration);
  EXPECT_EQ(0, manager_.media_metadata_.width);
  EXPECT_EQ(0, manager_.media_metadata_.height);
}

TEST_F(MediaCodecPlayerTest, SetAudioVideoConfigsAfterPlayerCreation) {
  CreatePlayer();

  // Wait till the player is initialized on media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(&MockDemuxerAndroid::IsInitialized,
                                          base::Unretained(demuxer_))));

  // Post configuration after the player has been initialized.
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);
  demuxer_->PostConfigs(CreateAudioVideoConfigs(duration, gfx::Size(320, 240)));

  // Configuration should propagate through the player and to the manager.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsMetadataChanged,
                                  base::Unretained(&manager_))));

  EXPECT_EQ(duration, manager_.media_metadata_.duration);
  EXPECT_EQ(320, manager_.media_metadata_.width);
  EXPECT_EQ(240, manager_.media_metadata_.height);
}

TEST_F(MediaCodecPlayerTest, AudioPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(2000);

  demuxer_->SetAudioFactory(
      scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  CreatePlayer();

  // Wait till the player is initialized on media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(&MockDemuxerAndroid::IsInitialized,
                                          base::Unretained(demuxer_))));

  // Post configuration after the player has been initialized.
  demuxer_->PostInternalConfigs();

  EXPECT_FALSE(manager_.IsPlaybackCompleted());

  player_->Start();

  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackCompleted,
                                  base::Unretained(&manager_)),
                       timeout));

  // Current timestamp reflects "now playing" time. It might come with delay
  // relative to the frame's PTS. Allow for 100 ms delay here.
  base::TimeDelta audio_pts_delay = base::TimeDelta::FromMilliseconds(100);
  EXPECT_LT(duration - audio_pts_delay, manager_.pts_stat_.max());
}

TEST_F(MediaCodecPlayerTest, VideoPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1500);

  demuxer_->SetVideoFactory(
      scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  CreatePlayer();
  SetVideoSurface();

  // Wait till the player is initialized on media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(&MockDemuxerAndroid::IsInitialized,
                                          base::Unretained(demuxer_))));

  // Post configuration after the player has been initialized.
  demuxer_->PostInternalConfigs();

  EXPECT_FALSE(manager_.IsPlaybackCompleted());

  player_->Start();

  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackCompleted,
                                  base::Unretained(&manager_)),
                       timeout));

  EXPECT_LE(duration, manager_.pts_stat_.max());
}

}  // namespace media
