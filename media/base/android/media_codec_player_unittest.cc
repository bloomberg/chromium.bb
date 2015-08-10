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
#include "media/base/buffers.h"
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

// The predicate that always returns false, used for WaitForDelay implementation
bool AlwaysFalse() {
  return false;
}

// The method used to compare two TimeDelta values in expectations.
bool AlmostEqual(base::TimeDelta a, base::TimeDelta b, double tolerance_ms) {
  return (a - b).magnitude().InMilliseconds() <= tolerance_ms;
}

// Mock of MediaPlayerManager for testing purpose.

class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  MockMediaPlayerManager()
      : playback_completed_(false),
        num_seeks_completed_(0),
        weak_ptr_factory_(this) {}
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
                      const base::TimeDelta& current_time) override {
    ++num_seeks_completed_;
  }
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
  bool IsPlaybackStarted() const { return pts_stat_.num_values() > 0; }
  bool IsPlaybackBeyondPosition(const base::TimeDelta& pts) const {
    return pts_stat_.max() > pts;
  }
  bool IsSeekCompleted() const { return num_seeks_completed_ > 0; }

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
  int num_seeks_completed_;

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
      : TestDataFactory("h264-320x180-frame-%d", duration, kVideoFramePeriod),
        key_frame_requested_(true) {}

  DemuxerConfigs GetConfigs() const override {
    return TestDataFactory::CreateVideoConfigs(kCodecH264, duration_,
                                               gfx::Size(320, 180));
  }

  void RequestKeyFrame() { key_frame_requested_ = true; }

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
        unit->is_key_frame = key_frame_requested_;
        key_frame_requested_ = false;
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

 private:
  bool key_frame_requested_;
};

// Mock of DemuxerAndroid for testing purpose.

class MockDemuxerAndroid : public DemuxerAndroid {
 public:
  MockDemuxerAndroid()
      : client_(nullptr), num_seeks_(0), num_browser_seeks_(0) {}
  ~MockDemuxerAndroid() override {}

  // DemuxerAndroid implementation
  void Initialize(DemuxerAndroidClient* client) override;
  void RequestDemuxerData(DemuxerStream::Type type) override;
  void RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                          bool is_browser_seek) override;

  // Sets the audio data factory.
  void SetAudioFactory(scoped_ptr<AudioFactory> factory) {
    audio_factory_ = factory.Pass();
  }

  // Sets the video data factory.
  void SetVideoFactory(scoped_ptr<VideoFactory> factory) {
    video_factory_ = factory.Pass();
  }

  // Sets the delay in OnDemuxerSeekDone response.
  void SetSeekDoneDelay(base::TimeDelta delay) { seek_done_delay_ = delay; }

  // Post DemuxerConfigs to the client (i.e. the player) on correct thread.
  void PostConfigs(const DemuxerConfigs& configs);

  // Post DemuxerConfigs derived from data factories that has been set.
  void PostInternalConfigs();

  // Conditions to wait for.
  bool IsInitialized() const { return client_; }
  bool HasPendingConfigs() const { return pending_configs_; }
  bool ReceivedSeekRequest() const { return num_seeks_ > 0; }
  bool ReceivedBrowserSeekRequest() const { return num_browser_seeks_ > 0; }

 private:
  DemuxerAndroidClient* client_;
  scoped_ptr<DemuxerConfigs> pending_configs_;
  scoped_ptr<AudioFactory> audio_factory_;
  scoped_ptr<VideoFactory> video_factory_;
  base::TimeDelta seek_done_delay_;
  int num_seeks_;
  int num_browser_seeks_;

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

void MockDemuxerAndroid::RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                                            bool is_browser_seek) {
  // Tell data factories to start next chunk with the new timestamp.
  if (audio_factory_)
    audio_factory_->SeekTo(time_to_seek);
  if (video_factory_) {
    video_factory_->SeekTo(time_to_seek);
    video_factory_->RequestKeyFrame();
  }

  ++num_seeks_;
  if (is_browser_seek)
    ++num_browser_seeks_;

  // Post OnDemuxerSeekDone() to the player.
  DCHECK(client_);
  base::TimeDelta reported_seek_time =
      is_browser_seek ? time_to_seek : kNoTimestamp();
  GetMediaTaskRunner()->PostDelayedTask(
      FROM_HERE, base::Bind(&DemuxerAndroidClient::OnDemuxerSeekDone,
                            base::Unretained(client_), reported_seek_time),
      seek_done_delay_);
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

  // Conditions to wait for.
  bool IsPaused() const { return !(player_ && player_->IsPlaying()); }

 protected:
  typedef base::Callback<bool()> Predicate;

  void CreatePlayer();
  void SetVideoSurface();
  void SetVideoSurfaceB();
  void RemoveVideoSurface();

  // Waits for condition to become true or for timeout to expire.
  // Returns true if the condition becomes true.
  bool WaitForCondition(const Predicate& condition,
                        const base::TimeDelta& timeout = kDefaultTimeout);

  // Waits for timeout to expire.
  void WaitForDelay(const base::TimeDelta& timeout);

  // Waits till playback position as determined by maximal reported pts
  // reaches the given value or for timeout to expire. Returns true if the
  // playback has passed the given position.
  bool WaitForPlaybackBeyondPosition(
      const base::TimeDelta& pts,
      const base::TimeDelta& timeout = kDefaultTimeout);

  // Helper method that starts video only stream. Waits till it actually
  // started.
  void StartVideoPlayback(base::TimeDelta duration);

  base::MessageLoop message_loop_;
  MockMediaPlayerManager manager_;
  MockDemuxerAndroid* demuxer_;  // owned by player_
  scoped_refptr<gfx::SurfaceTexture> surface_texture_a_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_b_;
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
  surface_texture_a_ = gfx::SurfaceTexture::Create(0);
  gfx::ScopedJavaSurface surface(surface_texture_a_.get());

  ASSERT_NE(nullptr, player_);
  player_->SetVideoSurface(surface.Pass());
}

void MediaCodecPlayerTest::SetVideoSurfaceB() {
  surface_texture_b_ = gfx::SurfaceTexture::Create(1);
  gfx::ScopedJavaSurface surface(surface_texture_b_.get());

  ASSERT_NE(nullptr, player_);
  player_->SetVideoSurface(surface.Pass());
}

void MediaCodecPlayerTest::RemoveVideoSurface() {
  player_->SetVideoSurface(gfx::ScopedJavaSurface());
  surface_texture_a_ = NULL;
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

void MediaCodecPlayerTest::WaitForDelay(const base::TimeDelta& timeout) {
  WaitForCondition(base::Bind(&AlwaysFalse), timeout);
}

bool MediaCodecPlayerTest::WaitForPlaybackBeyondPosition(
    const base::TimeDelta& pts,
    const base::TimeDelta& timeout) {
  return WaitForCondition(
      base::Bind(&MockMediaPlayerManager::IsPlaybackBeyondPosition,
                 base::Unretained(&manager_), pts),
      timeout);
}

void MediaCodecPlayerTest::StartVideoPlayback(base::TimeDelta duration) {
  const base::TimeDelta start_timeout = base::TimeDelta::FromMilliseconds(800);

  demuxer_->SetVideoFactory(
      scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  CreatePlayer();
  SetVideoSurface();

  // Wait till the player is initialized on media thread.
  EXPECT_TRUE(WaitForCondition(base::Bind(&MockDemuxerAndroid::IsInitialized,
                                          base::Unretained(demuxer_))));

  // Post configuration after the player has been initialized.
  demuxer_->PostInternalConfigs();

  // Start the player.
  EXPECT_FALSE(manager_.IsPlaybackStarted());
  player_->Start();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_)),
                       start_timeout));
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

  StartVideoPlayback(duration);

  // Wait till completion.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackCompleted,
                                  base::Unretained(&manager_)),
                       timeout));

  EXPECT_LE(duration, manager_.pts_stat_.max());
}

// http://crbug.com/518900
TEST_F(MediaCodecPlayerTest, DISABLED_AudioSeekAfterStop) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Play for 300 ms, then Pause, then Seek to beginning. The playback should
  // start from the beginning.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(2000);

  demuxer_->SetAudioFactory(
      scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  CreatePlayer();

  // Post configuration.
  demuxer_->PostInternalConfigs();

  // Start the player.
  player_->Start();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_))));

  // Wait for 300 ms and stop. The 300 ms interval takes into account potential
  // audio delay: audio takes time reconfiguring after the first several packets
  // get written to the audio track.
  WaitForDelay(base::TimeDelta::FromMilliseconds(300));

  player_->Pause(true);

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Wait till the Pause is completed.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecPlayerTest::IsPaused, base::Unretained(this))));

  // Clear statistics.
  manager_.pts_stat_.Clear();

  // Now we can seek to the beginning and start the playback.
  player_->SeekTo(base::TimeDelta());

  player_->Start();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_))));

  // Make sure we started from the beginninig
  EXPECT_GT(base::TimeDelta::FromMilliseconds(40), manager_.pts_stat_.min());

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));
}

TEST_F(MediaCodecPlayerTest, AudioSeekThenPlay) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Issue Seek command immediately followed by Start. The playback should
  // start at the seek position.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(2000);
  base::TimeDelta seek_position = base::TimeDelta::FromMilliseconds(500);

  demuxer_->SetAudioFactory(
      scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  CreatePlayer();

  // Post configuration.
  demuxer_->PostInternalConfigs();

  // Seek and immediately start.
  player_->SeekTo(seek_position);
  player_->Start();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_))));

  // The playback should start at |seek_position|
  EXPECT_TRUE(AlmostEqual(seek_position, manager_.pts_stat_.min(), 1));

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));
}

TEST_F(MediaCodecPlayerTest, AudioSeekThenPlayThenConfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Issue Seek command immediately followed by Start but without prior demuxer
  // configuration. Start should wait for configuration. After it has been
  // posted the playback should start at the seek position.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(2000);
  base::TimeDelta seek_position = base::TimeDelta::FromMilliseconds(500);

  demuxer_->SetAudioFactory(
      scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  CreatePlayer();

  // Seek and immediately start.
  player_->SeekTo(seek_position);
  player_->Start();

  // Make sure the player is waiting.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(player_->IsPlaying());

  // Post configuration.
  demuxer_->PostInternalConfigs();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_))));

  // The playback should start at |seek_position|
  EXPECT_TRUE(AlmostEqual(seek_position, manager_.pts_stat_.min(), 1));

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));
}

// http://crbug.com/518900
TEST_F(MediaCodecPlayerTest, DISABLED_AudioSeekWhilePlaying) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Play for 300 ms, then issue several Seek commands in the row.
  // The playback should continue at the last seek position.

  // To test this condition without analyzing the reported time details
  // and without introducing dependency on implementation I make a long (10s)
  // duration and test that the playback resumes after big time jump (5s) in a
  // short period of time (200 ms).
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  demuxer_->SetAudioFactory(
      scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  CreatePlayer();

  // Post configuration.
  demuxer_->PostInternalConfigs();

  // Start the player.
  player_->Start();

  // Wait for playback to start.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_))));

  // Wait for 300 ms.
  WaitForDelay(base::TimeDelta::FromMilliseconds(300));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Seek forward several times.
  player_->SeekTo(base::TimeDelta::FromSeconds(3));
  player_->SeekTo(base::TimeDelta::FromSeconds(4));
  player_->SeekTo(base::TimeDelta::FromSeconds(5));

  // Make sure that we reached the last timestamp within default timeout,
  // i.e. 200 ms.
  EXPECT_TRUE(WaitForPlaybackBeyondPosition(base::TimeDelta::FromSeconds(5)));
  EXPECT_TRUE(player_->IsPlaying());

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));
}

TEST_F(MediaCodecPlayerTest, VideoReplaceSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1500);

  StartVideoPlayback(duration);

  // Wait for some time and check statistics.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Set new video surface without removing the old one.
  SetVideoSurfaceB();

  // We should receive a browser seek request.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MockDemuxerAndroid::ReceivedBrowserSeekRequest,
                 base::Unretained(demuxer_))));

  // Playback should continue with a new surface. Wait till completion.
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackCompleted,
                                  base::Unretained(&manager_)),
                       timeout));
  EXPECT_LE(duration, manager_.pts_stat_.max());
}

TEST_F(MediaCodecPlayerTest, VideoRemoveAndSetSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(1000);

  StartVideoPlayback(duration);

  // Wait for some time and check statistics.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Remove video surface.
  RemoveVideoSurface();

  // We should be stuck waiting for the new surface.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(player_->IsPlaying());

  // Save last PTS and clear statistics.
  base::TimeDelta max_pts_before_removal = manager_.pts_stat_.max();
  manager_.pts_stat_.Clear();

  // After clearing statistics we are ready to wait for IsPlaybackStarted again.
  EXPECT_FALSE(manager_.IsPlaybackStarted());

  // Extra RemoveVideoSurface() should not change anything.
  RemoveVideoSurface();

  // Set another video surface.
  SetVideoSurfaceB();

  // We should receive a browser seek request.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MockDemuxerAndroid::ReceivedBrowserSeekRequest,
                 base::Unretained(demuxer_))));

  // Playback should continue with a new surface. Wait till it starts again.
  base::TimeDelta reconfigure_timeout = base::TimeDelta::FromMilliseconds(800);
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_)),
                       reconfigure_timeout));

  // Timestamps should not go back.
  EXPECT_LE(max_pts_before_removal, manager_.pts_stat_.max());
}

// http://crbug.com/518900
TEST_F(MediaCodecPlayerTest, DISABLED_VideoReleaseAndStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(1000);

  StartVideoPlayback(duration);

  // Wait for some time and check statistics.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // When the user presses Tasks button Chrome calls Pause() and Release().
  player_->Pause(true);
  player_->Release();

  // Make sure we are not playing any more.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(player_->IsPlaying());

  // Save last PTS and clear statistics.
  base::TimeDelta max_pts_before_backgrounding = manager_.pts_stat_.max();
  manager_.pts_stat_.Clear();

  // After clearing statistics we are ready to wait for IsPlaybackStarted again.
  EXPECT_FALSE(manager_.IsPlaybackStarted());

  // Restart.
  SetVideoSurface();
  player_->Start();

  // We should receive a browser seek request.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MockDemuxerAndroid::ReceivedBrowserSeekRequest,
                 base::Unretained(demuxer_))));

  // Wait for playback to start again.
  base::TimeDelta reconfigure_timeout = base::TimeDelta::FromMilliseconds(800);
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_)),
                       reconfigure_timeout));

  // Timestamps should not go back.
  EXPECT_LE(max_pts_before_backgrounding, manager_.pts_stat_.max());
}

TEST_F(MediaCodecPlayerTest, VideoSeekAndRelease) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(2000);
  base::TimeDelta seek_position = base::TimeDelta::FromMilliseconds(1000);

  StartVideoPlayback(duration);

  // Wait for some time and check statistics.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Issue SeekTo() immediately followed by Release().
  player_->SeekTo(seek_position);
  player_->Release();

  // Make sure we are not playing any more.
  WaitForDelay(base::TimeDelta::FromMilliseconds(400));
  EXPECT_FALSE(player_->IsPlaying());

  // The Release() should not cancel the SeekTo() and we should have received
  // the seek request by this time.
  EXPECT_TRUE(demuxer_->ReceivedSeekRequest());

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));

  // Clear statistics.
  manager_.pts_stat_.Clear();

  // After clearing statistics we are ready to wait for IsPlaybackStarted again.
  EXPECT_FALSE(manager_.IsPlaybackStarted());

  // Restart.
  SetVideoSurface();
  player_->Start();

  // Wait for playback to start again.
  base::TimeDelta reconfigure_timeout = base::TimeDelta::FromMilliseconds(800);
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_)),
                       reconfigure_timeout));

  // Timestamps should start at the new seek position
  EXPECT_LE(seek_position, manager_.pts_stat_.min());
}

TEST_F(MediaCodecPlayerTest, VideoReleaseWhileWaitingForSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(2000);
  base::TimeDelta seek_position = base::TimeDelta::FromMilliseconds(1000);

  StartVideoPlayback(duration);

  // Wait for some time and check statistics.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  // Make sure we played at least 100 ms.
  EXPECT_LT(base::TimeDelta::FromMilliseconds(100), manager_.pts_stat_.max());

  // Set artificial delay in the OnDemuxerSeekDone response so we can
  // issue commands while the player is in the STATE_WAITING_FOR_SEEK.
  demuxer_->SetSeekDoneDelay(base::TimeDelta::FromMilliseconds(100));

  // Issue SeekTo().
  player_->SeekTo(seek_position);

  // Wait for the seek request to demuxer.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockDemuxerAndroid::ReceivedSeekRequest, base::Unretained(demuxer_))));

  // The player is supposed to be in STATE_WAITING_FOR_SEEK. Issue Release().
  player_->Release();

  // Make sure we are not playing any more.
  WaitForDelay(base::TimeDelta::FromMilliseconds(400));
  EXPECT_FALSE(player_->IsPlaying());

  // Clear statistics.
  manager_.pts_stat_.Clear();

  // After clearing statistics we are ready to wait for IsPlaybackStarted again.
  EXPECT_FALSE(manager_.IsPlaybackStarted());

  // Restart.
  SetVideoSurface();
  player_->Start();

  // Wait for playback to start again.
  base::TimeDelta reconfigure_timeout = base::TimeDelta::FromMilliseconds(1000);
  EXPECT_TRUE(
      WaitForCondition(base::Bind(&MockMediaPlayerManager::IsPlaybackStarted,
                                  base::Unretained(&manager_)),
                       reconfigure_timeout));

  // Timestamps should start at the new seek position
  EXPECT_LE(seek_position, manager_.pts_stat_.min());

  // The player should have reported the seek completion to the manager.
  EXPECT_TRUE(WaitForCondition(base::Bind(
      &MockMediaPlayerManager::IsSeekCompleted, base::Unretained(&manager_))));
}

}  // namespace media
