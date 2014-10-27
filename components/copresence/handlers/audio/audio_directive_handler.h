// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/copresence/handlers/audio/audio_directive_list.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/proto/data.pb.h"

namespace base {
class Timer;
}

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class TickClockRefCounted;

// The AudioDirectiveHandler handles audio transmit and receive instructions.
// TODO(rkc): Currently since WhispernetClient can only have one token encoded
// callback at a time, we need to have both the audible and inaudible in this
// class. Investigate a better way to do this; a few options are abstracting
// out token encoding to a separate class, or allowing whispernet to have
// multiple callbacks for encoded tokens being sent back and have two versions
// of this class.
class AudioDirectiveHandler final {
 public:
  AudioDirectiveHandler();
  ~AudioDirectiveHandler();

  // Do not use this class before calling this.
  void Initialize(const AudioManager::DecodeSamplesCallback& decode_cb,
                  const AudioManager::EncodeTokenCallback& encode_cb);

  // Adds an instruction to our handler. The instruction will execute and be
  // removed after the ttl expires.
  void AddInstruction(const copresence::TokenInstruction& instruction,
                      const std::string& op_id,
                      base::TimeDelta ttl_ms);

  // Removes all instructions associated with this operation id.
  void RemoveInstructions(const std::string& op_id);

  // Returns the currently playing token.
  const std::string PlayingToken(AudioType type) const;

  // The manager being passed in needs to be uninitialized.
  void set_audio_manager_for_testing(scoped_ptr<AudioManager> manager) {
    audio_manager_ = manager.Pass();
  }

  void set_clock_for_testing(const scoped_refptr<TickClockRefCounted>& clock);
  void set_timer_for_testing(scoped_ptr<base::Timer> timer);

 private:
  // Processes the next active instruction, updating our audio manager state
  // accordingly.
  void ProcessNextInstruction();

  // Returns the time that an instruction expires at. This will always return
  // the earliest expiry time among all the active receive and transmit
  // instructions. In case we don't have any active instructions, this method
  // returns false.
  bool GetNextInstructionExpiry(base::TimeTicks* next_event);

  scoped_ptr<AudioManager> audio_manager_;

  // Audible and inaudible lists.
  // AUDIBLE = 0, INAUDIBLE = 1 (see copresence_constants.h).
  AudioDirectiveList transmits_list_[2];
  AudioDirectiveList receives_list_[2];

  scoped_ptr<base::Timer> audio_event_timer_;

  scoped_refptr<TickClockRefCounted> clock_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
