// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/test/fake_directive_handler.h"

#include "components/copresence/proto/data.pb.h"

namespace copresence {

FakeDirectiveHandler::FakeDirectiveHandler() {}

FakeDirectiveHandler::~FakeDirectiveHandler() {}

void FakeDirectiveHandler::AddDirective(const Directive& directive) {
  added_directives_.push_back(directive.subscription_id());
}

void FakeDirectiveHandler::RemoveDirectives(const std::string& op_id) {
  removed_directives_.push_back(op_id);
}

const std::string
FakeDirectiveHandler::GetCurrentAudioToken(audio_modem::AudioType type) const {
  return type == audio_modem::AUDIBLE ? "current audible" : "current inaudible";
}

bool FakeDirectiveHandler::IsAudioTokenHeard(
    audio_modem::AudioType type) const {
  return true;
}

}  // namespace copresence
