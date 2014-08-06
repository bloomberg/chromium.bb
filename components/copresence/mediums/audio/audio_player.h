// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_
#define COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_io.h"

namespace media {
class AudioBus;
class AudioBusRefCounted;
}

namespace copresence {

// The AudioPlayer class will play a set of samples till it is told to stop.
class AudioPlayer : public media::AudioOutputStream::AudioSourceCallback {
 public:
  AudioPlayer();

  // Initializes the object. Do not use this object before calling this method.
  void Initialize();

  // Play the given samples. These samples will keep on being played in a loop
  // till we explicitly tell the player to stop playing.
  void Play(const scoped_refptr<media::AudioBusRefCounted>& samples);

  // Stop playing.
  void Stop();

  // Cleans up and deletes this object. Do not use object after this call.
  void Finalize();

  // Takes ownership of the stream.
  void set_output_stream_for_testing(
      media::AudioOutputStream* output_stream_for_testing) {
    output_stream_for_testing_.reset(output_stream_for_testing);
  }

 private:
  friend class AudioPlayerTest;
  FRIEND_TEST_ALL_PREFIXES(AudioPlayerTest, BasicPlayAndStop);
  FRIEND_TEST_ALL_PREFIXES(AudioPlayerTest, OutOfOrderPlayAndStopMultiple);

  virtual ~AudioPlayer();

  // Methods to do our various operations; all of these need to be run on the
  // audio thread.
  void InitializeOnAudioThread();
  void PlayOnAudioThread(
      const scoped_refptr<media::AudioBusRefCounted>& samples);
  void StopOnAudioThread();
  void StopAndCloseOnAudioThread();
  void FinalizeOnAudioThread();

  // AudioOutputStream::AudioSourceCallback overrides:
  // Following methods could be called from *ANY* thread.
  virtual int OnMoreData(media::AudioBus* dest,
                         media::AudioBuffersState /* state */) OVERRIDE;
  virtual void OnError(media::AudioOutputStream* /* stream */) OVERRIDE;

  // Flushes the audio loop, making sure that any queued operations are
  // performed.
  void FlushAudioLoopForTesting();

  // Self-deleting object.
  media::AudioOutputStream* stream_;

  scoped_ptr<media::AudioOutputStream> output_stream_for_testing_;

  bool is_playing_;

  // All fields below here are protected by this lock.
  base::Lock state_lock_;

  scoped_refptr<media::AudioBusRefCounted> samples_;

  // Index to the frame in the samples that we need to play next.
  int frame_index_;

  DISALLOW_COPY_AND_ASSIGN(AudioPlayer);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_
