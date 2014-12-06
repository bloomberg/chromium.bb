// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_IMPL_H_
#define COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/timed_map.h"

namespace base {
class Time;
}

namespace copresence {

class AudioPlayer;
class AudioRecorder;
class WhispernetClient;

// The AudioManagerImpl class manages the playback and recording of tokens. Once
// playback or recording is started, it is up to the audio manager to handle
// the specifics of how this is done. In the future, for example, this class
// may pause recording and playback to implement carrier sense.
class AudioManagerImpl final : public AudioManager {
 public:
  AudioManagerImpl();
  ~AudioManagerImpl() override;

  // AudioManager overrides:
  void Initialize(WhispernetClient* whispernet_client,
                  const TokensCallback& tokens_cb) override;
  void StartPlaying(AudioType type) override;
  void StopPlaying(AudioType type) override;
  void StartRecording(AudioType type) override;
  void StopRecording(AudioType type) override;
  void SetToken(AudioType type, const std::string& url_unsafe_token) override;
  const std::string GetToken(AudioType type) override;
  bool IsRecording(AudioType type) override;
  bool IsPlaying(AudioType type) override;
  bool IsPlayingTokenHeard(AudioType type) override;

  void set_player_for_testing(AudioType type, AudioPlayer* player) {
    player_[type] = player;
  }
  void set_recorder_for_testing(AudioRecorder* recorder) {
    recorder_ = recorder;
  }

 private:
  typedef TimedMap<std::string, scoped_refptr<media::AudioBusRefCounted>>
      SamplesMap;

  // Receives the audio samples from encoding a token.
  void OnTokenEncoded(AudioType type,
                      const std::string& token,
                      const scoped_refptr<media::AudioBusRefCounted>& samples);

  // Receives any tokens found by decoding audio samples.
  void OnTokensFound(const std::vector<AudioToken>& tokens);

  // Update our currently playing token with the new token. Change the playing
  // samples if needed. Prerequisite: Samples corresponding to this token
  // should already be in the samples cache.
  void UpdateToken(AudioType type, const std::string& token);

  WhispernetClient* whispernet_client_;

  // Callbacks to send tokens back to the CopresenceManager.
  TokensCallback tokens_cb_;

  // This cancelable callback is passed to the recorder. The recorder's
  // destruction will happen on the audio thread, so it can outlive us.
  base::CancelableCallback<void(const std::string&)> decode_cancelable_cb_;

  // We use the AudioType enum to index into all our data structures that work
  // on values for both audible and inaudible.
  static_assert(AUDIBLE == 0, "AudioType::AUDIBLE should be 0.");
  static_assert(INAUDIBLE == 1, "AudioType::INAUDIBLE should be 1.");

  // Indexed using enum AudioType.
  bool playing_[2];
  bool recording_[2];

  // AudioPlayer and AudioRecorder objects are self-deleting. When we call
  // Finalize on them, they clean themselves up on the Audio thread.
  // Indexed using enum AudioType.
  AudioPlayer* player_[2];
  AudioRecorder* recorder_;

  // Indexed using enum AudioType.
  std::string playing_token_[2];
  base::Time started_playing_[2];
  base::Time heard_own_token_[2];

  // Cache that holds the encoded samples. After reaching its limit, the cache
  // expires the oldest samples first.
  // Indexed using enum AudioType.
  ScopedVector<SamplesMap> samples_cache_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_IMPL_H_
