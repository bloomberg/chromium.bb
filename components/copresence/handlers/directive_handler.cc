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
    const AudioDirectiveList::EncodeTokenCallback& encode_cb) {
  audio_handler_.reset(new AudioDirectiveHandler(decode_cb, encode_cb));
  audio_handler_->Initialize();
}

DirectiveHandler::~DirectiveHandler() {
}

void DirectiveHandler::AddDirective(const Directive& directive) {
  // We only handle Token directives; wifi/ble requests aren't implemented.
  DCHECK_EQ(directive.instruction_type(), TOKEN);

  const TokenInstruction& ti = directive.token_instruction();
  // We currently only support audio.
  DCHECK_EQ(ti.medium(), AUDIO_ULTRASOUND_PASSBAND);
  DCHECK(audio_handler_.get()) << "Clients must call Initialize() before "
                               << "any other DirectiveHandler methods.";
  audio_handler_->AddInstruction(
      ti, base::TimeDelta::FromMilliseconds(directive.ttl_millis()));
}

void DirectiveHandler::RemoveDirectives(const std::string& /* op_id */) {
  // TODO(rkc): Forward the remove directive call to all the directive handlers.
  DCHECK(audio_handler_.get()) << "Clients must call Initialize() before "
                               << "any other DirectiveHandler methods.";
}

}  // namespace copresence
