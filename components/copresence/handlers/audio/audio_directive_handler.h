// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_

#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/copresence/handlers/audio/audio_directive_list.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/proto/data.pb.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class AudioPlayer;

// The AudioDirectiveHandler handles audio transmit and receive instructions.
class AudioDirectiveHandler {
 public:
  AudioDirectiveHandler(
      const AudioRecorder::DecodeSamplesCallback& decode_cb,
      const AudioDirectiveList::EncodeTokenCallback& encode_cb);
  virtual ~AudioDirectiveHandler();

  // Do not use this class before calling this.
  void Initialize();

  // Adds an instruction to our handler. The instruction will execute and be
  // removed after the ttl expires.
  void AddInstruction(const copresence::TokenInstruction& instruction,
                      base::TimeDelta ttl_ms);

 protected:
  // Protected and virtual since we want to be able to mock these out.
  virtual void PlayAudio(
      const scoped_refptr<media::AudioBusRefCounted>& samples,
      base::TimeDelta duration);
  virtual void RecordAudio(base::TimeDelta duration);

 private:
  void StopPlayback();
  void StopRecording();

  // Execute the next active transmit instruction.
  void ExecuteNextTransmit();
  // Execute the next active receive instruction.
  void ExecuteNextReceive();

  AudioDirectiveList directive_list_;

  // The next two pointers are self-deleting. When we call Finalize on them,
  // they clean themselves up on the Audio thread.
  AudioPlayer* player_;
  AudioRecorder* recorder_;

  AudioRecorder::DecodeSamplesCallback decode_cb_;

  base::OneShotTimer<AudioDirectiveHandler> stop_playback_timer_;
  base::OneShotTimer<AudioDirectiveHandler> stop_recording_timer_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_
