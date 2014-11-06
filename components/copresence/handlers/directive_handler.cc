// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/directive_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"
#include "components/copresence/proto/data.pb.h"

namespace copresence {

// Public functions

DirectiveHandler::DirectiveHandler(
    scoped_ptr<AudioDirectiveHandler> audio_handler)
    : audio_handler_(audio_handler.Pass()),
      whispernet_client_(nullptr) {}

DirectiveHandler::~DirectiveHandler() {}

void DirectiveHandler::Start(WhispernetClient* whispernet_client) {
  DCHECK(whispernet_client);
  whispernet_client_ = whispernet_client;

  // TODO(ckehoe): Just pass Whispernet all the way down to the AudioManager.
  //               We shouldn't be concerned with these details here.
  audio_handler_->Initialize(
      base::Bind(&WhispernetClient::DecodeSamples,
                 base::Unretained(whispernet_client_)),
      base::Bind(&DirectiveHandler::EncodeToken,
                 base::Unretained(this)));

  // Run all the queued directives.
  for (const auto& op_id : pending_directives_) {
    for (const Directive& directive : op_id.second) {
      StartDirective(op_id.first, directive);
    }
  }
  pending_directives_.clear();
}

void DirectiveHandler::AddDirective(const Directive& directive) {
  // We only handle transmit and receive directives.
  // WiFi and BLE scans aren't implemented.
  DCHECK_EQ(directive.instruction_type(), TOKEN);

  std::string op_id = directive.published_message_id();
  if (op_id.empty())
    op_id = directive.subscription_id();
  if (op_id.empty()) {
    NOTREACHED() << "No operation associated with directive!";
    return;
  }

  if (!whispernet_client_) {
    pending_directives_[op_id].push_back(directive);
  } else {
    StartDirective(op_id, directive);
  }
}

void DirectiveHandler::RemoveDirectives(const std::string& op_id) {
  // If whispernet_client_ is null, audio_handler_ hasn't been Initialized.
  if (whispernet_client_) {
    audio_handler_->RemoveInstructions(op_id);
  } else {
    pending_directives_.erase(op_id);
  }
}

const std::string DirectiveHandler::GetCurrentAudioToken(AudioType type) const {
  // If whispernet_client_ is null, audio_handler_ hasn't been Initialized.
  return whispernet_client_ ? audio_handler_->PlayingToken(type) : "";
}


// Private functions

void DirectiveHandler::StartDirective(const std::string& op_id,
                                      const Directive& directive) {
  DCHECK(whispernet_client_);
  const TokenInstruction& ti = directive.token_instruction();
  if (ti.medium() == AUDIO_ULTRASOUND_PASSBAND ||
      ti.medium() == AUDIO_AUDIBLE_DTMF) {
    audio_handler_->AddInstruction(
        ti, op_id, base::TimeDelta::FromMilliseconds(directive.ttl_millis()));
  } else {
    // We should only get audio directives.
    NOTREACHED() << "Received directive for unimplemented medium "
                 << ti.medium();
  }
}

// TODO(ckehoe): We don't need to re-register the samples callback
// every time. Which means this whole function is unnecessary.
void DirectiveHandler::EncodeToken(
    const std::string& token,
    AudioType type,
    const WhispernetClient::SamplesCallback& samples_callback) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  // TODO(ckehoe): This null check shouldn't be necessary.
  // It's only here for tests.
  if (whispernet_client_) {
    whispernet_client_->RegisterSamplesCallback(samples_callback);
    whispernet_client_->EncodeToken(token, type);
  }
}

}  // namespace copresence
