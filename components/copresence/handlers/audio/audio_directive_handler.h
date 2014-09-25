// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/copresence/handlers/audio/audio_directive_list.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/timed_map.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class AudioPlayer;

// The AudioDirectiveHandler handles audio transmit and receive instructions.
// TODO(rkc): Currently since WhispernetClient can only have one token encoded
// callback at a time, we need to have both the audible and inaudible in this
// class. Investigate a better way to do this; a few options are abstracting
// out token encoding to a separate class, or allowing whispernet to have
// multiple callbacks for encoded tokens being sent back and have two versions
// of this class.
class AudioDirectiveHandler {
 public:
  typedef base::Callback<void(const std::string&,
                              bool,
                              const scoped_refptr<media::AudioBusRefCounted>&)>
      SamplesCallback;
  typedef base::Callback<void(const std::string&, bool, const SamplesCallback&)>
      EncodeTokenCallback;

  AudioDirectiveHandler(
      const AudioRecorder::DecodeSamplesCallback& decode_cb,
      const AudioDirectiveHandler::EncodeTokenCallback& encode_cb);
  virtual ~AudioDirectiveHandler();

  // Do not use this class before calling this.
  void Initialize();

  // Adds an instruction to our handler. The instruction will execute and be
  // removed after the ttl expires.
  void AddInstruction(const copresence::TokenInstruction& instruction,
                      const std::string& op_id,
                      base::TimeDelta ttl_ms);

  // Removes all instructions associated with this operation id.
  void RemoveInstructions(const std::string& op_id);

  // Returns the currently playing DTMF token.
  const std::string& PlayingAudibleToken() const {
    return current_token_audible_;
  }

  // Returns the currently playing DSSS token.
  const std::string& PlayingInaudibleToken() const {
    return current_token_inaudible_;
  }

  void set_player_audible_for_testing(AudioPlayer* player) {
    player_audible_ = player;
  }

  void set_player_inaudible_for_testing(AudioPlayer* player) {
    player_inaudible_ = player;
  }

  void set_recorder_for_testing(AudioRecorder* recorder) {
    recorder_ = recorder;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioDirectiveHandlerTest, Basic);

  typedef TimedMap<std::string, scoped_refptr<media::AudioBusRefCounted>>
      SamplesMap;

  // Processes the next active transmit instruction.
  void ProcessNextTransmit();
  // Processes the next active receive instruction.
  void ProcessNextReceive();

  void PlayToken(const std::string token, bool audible);

  // This is the method that the whispernet client needs to call to return
  // samples to us.
  void PlayEncodedToken(
      const std::string& token,
      bool audible,
      const scoped_refptr<media::AudioBusRefCounted>& samples);

  AudioDirectiveList transmits_list_audible_;
  AudioDirectiveList transmits_list_inaudible_;
  AudioDirectiveList receives_list_;

  // Currently playing tokens.
  std::string current_token_audible_;
  std::string current_token_inaudible_;

  // AudioPlayer and AudioRecorder objects are self-deleting. When we call
  // Finalize on them, they clean themselves up on the Audio thread.
  AudioPlayer* player_audible_;
  AudioPlayer* player_inaudible_;
  AudioRecorder* recorder_;

  AudioRecorder::DecodeSamplesCallback decode_cb_;
  EncodeTokenCallback encode_cb_;

  base::OneShotTimer<AudioDirectiveHandler> stop_audible_playback_timer_;
  base::OneShotTimer<AudioDirectiveHandler> stop_inaudible_playback_timer_;
  base::OneShotTimer<AudioDirectiveHandler> stop_recording_timer_;

  // Cache that holds the encoded samples. After reaching its limit, the cache
  // expires the oldest samples first.
  SamplesMap samples_cache_audible_;
  SamplesMap samples_cache_inaudible_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
