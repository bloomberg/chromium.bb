// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/directive_handler.h"

#include "base/time/time.h"
#include "components/copresence/handlers/audio/audio_directive_handler.h"
#include "components/copresence/proto/data.pb.h"

namespace copresence {

DirectiveHandler::DirectiveHandler() {}

void DirectiveHandler::Initialize(
    const AudioRecorder::DecodeSamplesCallback& decode_cb,
    const AudioDirectiveHandler::EncodeTokenCallback& encode_cb) {
  audio_handler_.reset(new AudioDirectiveHandler(decode_cb, encode_cb));
  audio_handler_->Initialize();
}

DirectiveHandler::~DirectiveHandler() {
}

void DirectiveHandler::AddDirective(const Directive& directive) {
  // We only handle Token directives; wifi/ble requests aren't implemented.
  DCHECK_EQ(directive.instruction_type(), TOKEN);

  std::string op_id;
  if (directive.has_published_message_id()) {
    op_id = directive.published_message_id();
  } else if (directive.has_subscription_id()) {
    op_id = directive.subscription_id();
  } else {
    NOTREACHED() << "No operation associated with directive!";
    return;
  }

  const TokenInstruction& ti = directive.token_instruction();
  DCHECK(audio_handler_.get()) << "Clients must call Initialize() before "
                               << "any other DirectiveHandler methods.";
  // We currently only support audio.
  if (ti.medium() == AUDIO_ULTRASOUND_PASSBAND ||
      ti.medium() == AUDIO_AUDIBLE_DTMF) {
    audio_handler_->AddInstruction(
        ti, op_id, base::TimeDelta::FromMilliseconds(directive.ttl_millis()));
  }
}

void DirectiveHandler::RemoveDirectives(const std::string& op_id) {
  DCHECK(audio_handler_.get()) << "Clients must call Initialize() before "
                               << "any other DirectiveHandler methods.";
  audio_handler_->RemoveInstructions(op_id);
}

const std::string& DirectiveHandler::CurrentAudibleToken() const {
  return audio_handler_->PlayingAudibleToken();
}

const std::string& DirectiveHandler::CurrentInaudibleToken() const {
  return audio_handler_->PlayingInaudibleToken();
}

}  // namespace copresence
