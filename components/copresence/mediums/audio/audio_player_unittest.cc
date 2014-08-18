// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/mediums/audio/audio_player.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/test/audio_test_support.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAudioOutputStream : public media::AudioOutputStream {
 public:
  typedef base::Callback<void(scoped_ptr<media::AudioBus>, int frames)>
      GatherSamplesCallback;
  TestAudioOutputStream(int default_frame_count,
                        int max_frame_count,
                        GatherSamplesCallback gather_callback)
      : default_frame_count_(default_frame_count),
        max_frame_count_(max_frame_count),
        gather_callback_(gather_callback),
        callback_(NULL) {
    caller_loop_ = base::MessageLoop::current();
  }

  virtual ~TestAudioOutputStream() {}

  virtual bool Open() OVERRIDE { return true; }
  virtual void Start(AudioSourceCallback* callback) OVERRIDE {
    callback_ = callback;
    GatherPlayedSamples();
  }
  virtual void Stop() OVERRIDE {}
  virtual void SetVolume(double volume) OVERRIDE {}
  virtual void GetVolume(double* volume) OVERRIDE {}
  virtual void Close() OVERRIDE {}

 private:
  void GatherPlayedSamples() {
    int frames = 0, total_frames = 0;
    do {
      // Call back into the player to get samples that it wants us to play.
      scoped_ptr<media::AudioBus> dest =
          media::AudioBus::Create(1, default_frame_count_);
      frames = callback_->OnMoreData(dest.get(), media::AudioBuffersState());
      total_frames += frames;
      // Send the samples given to us by the player to the gather callback.
      caller_loop_->PostTask(
          FROM_HERE, base::Bind(gather_callback_, base::Passed(&dest), frames));
    } while (frames && total_frames < max_frame_count_);
  }

  int default_frame_count_;
  int max_frame_count_;
  GatherSamplesCallback gather_callback_;
  AudioSourceCallback* callback_;
  base::MessageLoop* caller_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioOutputStream);
};

}  // namespace

namespace copresence {

class AudioPlayerTest : public testing::Test,
                        public base::SupportsWeakPtr<AudioPlayerTest> {
 public:
  AudioPlayerTest() : buffer_index_(0), player_(NULL) {
    if (!media::AudioManager::Get())
      media::AudioManager::CreateForTesting();
  }

  virtual ~AudioPlayerTest() { DeletePlayer(); }

  void CreatePlayer() {
    DeletePlayer();
    player_ = new AudioPlayer();
    player_->set_output_stream_for_testing(new TestAudioOutputStream(
        kDefaultFrameCount,
        kMaxFrameCount,
        base::Bind(&AudioPlayerTest::GatherSamples, AsWeakPtr())));
    player_->Initialize();
  }

  void DeletePlayer() {
    if (!player_)
      return;
    player_->Finalize();
    player_ = NULL;
  }

  void PlayAndVerifySamples(
      const scoped_refptr<media::AudioBusRefCounted>& samples) {
    buffer_ = media::AudioBus::Create(1, kMaxFrameCount);
    player_->Play(samples);
    player_->FlushAudioLoopForTesting();

    int differences = 0;
    for (int i = 0; i < samples->frames(); ++i)
      differences += (buffer_->channel(0)[i] != samples->channel(0)[i]);
    ASSERT_EQ(0, differences);

    buffer_.reset();
  }

  void GatherSamples(scoped_ptr<media::AudioBus> bus, int frames) {
    if (!buffer_.get())
      return;
    bus->CopyPartialFramesTo(0, frames, buffer_index_, buffer_.get());
    buffer_index_ += frames;
  }

 protected:
  bool IsPlaying() {
    player_->FlushAudioLoopForTesting();
    return player_->is_playing_;
  }

  static const int kDefaultFrameCount = 1024;
  static const int kMaxFrameCount = 1024 * 10;

  scoped_ptr<media::AudioBus> buffer_;
  int buffer_index_;

  AudioPlayer* player_;
  base::MessageLoop message_loop_;
};

TEST_F(AudioPlayerTest, BasicPlayAndStop) {
  CreatePlayer();
  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1, 7331);

  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());
  player_->Stop();
  EXPECT_FALSE(IsPlaying());
  player_->Play(samples);

  EXPECT_TRUE(IsPlaying());
  player_->Stop();
  EXPECT_FALSE(IsPlaying());
  player_->Play(samples);

  EXPECT_TRUE(IsPlaying());
  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  DeletePlayer();
}

TEST_F(AudioPlayerTest, OutOfOrderPlayAndStopMultiple) {
  CreatePlayer();
  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1, 1337);

  player_->Stop();
  player_->Stop();
  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  player_->Play(samples);
  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());

  player_->Stop();
  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  DeletePlayer();
}

TEST_F(AudioPlayerTest, PlayingEndToEnd) {
  const int kNumSamples = kDefaultFrameCount * 10;
  CreatePlayer();

  PlayAndVerifySamples(CreateRandomAudioRefCounted(0x1337, 1, kNumSamples));

  DeletePlayer();
}

}  // namespace copresence
