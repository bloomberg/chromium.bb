// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"
#include "components/copresence/mediums/audio/audio_manager_impl.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "media/base/audio_bus.h"

namespace copresence {

namespace {

base::TimeTicks GetEarliestEventTime(AudioDirectiveList* list,
                                     base::TimeTicks event_time) {
  if (!list->GetActiveDirective())
    return event_time;

  if (event_time.is_null())
    return list->GetActiveDirective()->end_time;
  else
    return std::min(list->GetActiveDirective()->end_time, event_time);
}

}  // namespace

// Public methods.

AudioDirectiveHandler::AudioDirectiveHandler()
    : audio_event_timer_(new base::OneShotTimer<AudioDirectiveHandler>),
      clock_(new TickClockRefCounted(
          make_scoped_ptr(new base::DefaultTickClock))) {
}

AudioDirectiveHandler::~AudioDirectiveHandler() {
}

void AudioDirectiveHandler::Initialize(
    const AudioManager::DecodeSamplesCallback& decode_cb,
    const AudioManager::EncodeTokenCallback& encode_cb) {
  if (!audio_manager_)
    audio_manager_.reset(new AudioManagerImpl());
  audio_manager_->Initialize(decode_cb, encode_cb);
}

void AudioDirectiveHandler::AddInstruction(const TokenInstruction& instruction,
                                           const std::string& op_id,
                                           base::TimeDelta ttl) {
  switch (instruction.token_instruction_type()) {
    case TRANSMIT:
      DVLOG(2) << "Audio Transmit Directive received. Token: "
               << instruction.token_id()
               << " with medium=" << instruction.medium()
               << " with TTL=" << ttl.InMilliseconds();
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          transmits_list_[INAUDIBLE].AddDirective(op_id, ttl);
          audio_manager_->SetToken(INAUDIBLE, instruction.token_id());
          break;
        case AUDIO_AUDIBLE_DTMF:
          transmits_list_[AUDIBLE].AddDirective(op_id, ttl);
          audio_manager_->SetToken(AUDIBLE, instruction.token_id());
          break;
        default:
          NOTREACHED();
      }
      break;
    case RECEIVE:
      DVLOG(2) << "Audio Receive Directive received."
               << " with medium=" << instruction.medium()
               << " with TTL=" << ttl.InMilliseconds();
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          receives_list_[INAUDIBLE].AddDirective(op_id, ttl);
          break;
        case AUDIO_AUDIBLE_DTMF:
          receives_list_[AUDIBLE].AddDirective(op_id, ttl);
          break;
        default:
          NOTREACHED();
      }
      break;
    case UNKNOWN_TOKEN_INSTRUCTION_TYPE:
    default:
      LOG(WARNING) << "Unknown Audio Transmit Directive received. type = "
                   << instruction.token_instruction_type();
  }
  ProcessNextInstruction();
}

void AudioDirectiveHandler::RemoveInstructions(const std::string& op_id) {
  transmits_list_[AUDIBLE].RemoveDirective(op_id);
  transmits_list_[INAUDIBLE].RemoveDirective(op_id);
  receives_list_[AUDIBLE].RemoveDirective(op_id);
  receives_list_[INAUDIBLE].RemoveDirective(op_id);

  ProcessNextInstruction();
}

const std::string AudioDirectiveHandler::PlayingToken(AudioType type) const {
  return audio_manager_->GetToken(type);
}

void AudioDirectiveHandler::set_clock_for_testing(
    const scoped_refptr<TickClockRefCounted>& clock) {
  clock_ = clock;

  transmits_list_[AUDIBLE].set_clock_for_testing(clock);
  transmits_list_[INAUDIBLE].set_clock_for_testing(clock);
  receives_list_[AUDIBLE].set_clock_for_testing(clock);
  receives_list_[INAUDIBLE].set_clock_for_testing(clock);
}

void AudioDirectiveHandler::set_timer_for_testing(
    scoped_ptr<base::Timer> timer) {
  audio_event_timer_.swap(timer);
}

// Private methods.

void AudioDirectiveHandler::ProcessNextInstruction() {
  DCHECK(audio_event_timer_);
  audio_event_timer_->Stop();

  // Change |audio_manager_| state for audible transmits.
  if (transmits_list_[AUDIBLE].GetActiveDirective())
    audio_manager_->StartPlaying(AUDIBLE);
  else
    audio_manager_->StopPlaying(AUDIBLE);

  // Change audio_manager_ state for inaudible transmits.
  if (transmits_list_[INAUDIBLE].GetActiveDirective())
    audio_manager_->StartPlaying(INAUDIBLE);
  else
    audio_manager_->StopPlaying(INAUDIBLE);

  // Change audio_manager_ state for audible receives.
  if (receives_list_[AUDIBLE].GetActiveDirective())
    audio_manager_->StartRecording(AUDIBLE);
  else
    audio_manager_->StopRecording(AUDIBLE);

  // Change audio_manager_ state for inaudible receives.
  if (receives_list_[INAUDIBLE].GetActiveDirective())
    audio_manager_->StartRecording(INAUDIBLE);
  else
    audio_manager_->StopRecording(INAUDIBLE);

  base::TimeTicks next_event_time;
  if (GetNextInstructionExpiry(&next_event_time)) {
    audio_event_timer_->Start(
        FROM_HERE,
        next_event_time - clock_->NowTicks(),
        base::Bind(&AudioDirectiveHandler::ProcessNextInstruction,
                   base::Unretained(this)));
  }
}

bool AudioDirectiveHandler::GetNextInstructionExpiry(base::TimeTicks* expiry) {
  DCHECK(expiry);

  *expiry = GetEarliestEventTime(&transmits_list_[AUDIBLE], base::TimeTicks());
  *expiry = GetEarliestEventTime(&transmits_list_[INAUDIBLE], *expiry);
  *expiry = GetEarliestEventTime(&receives_list_[AUDIBLE], *expiry);
  *expiry = GetEarliestEventTime(&receives_list_[INAUDIBLE], *expiry);

  return !expiry->is_null();
}

}  // namespace copresence
