// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence/handlers/audio/audio_directive_list.h"
#include "components/copresence/mediums/audio/audio_recorder.h"

namespace copresence {

class AudioDirectiveHandler;
class Directive;

// The directive handler manages transmit and receive directives given to it
// by the client.
class DirectiveHandler {
 public:
  DirectiveHandler(const AudioRecorder::DecodeSamplesCallback& decode_cb,
                   const AudioDirectiveList::EncodeTokenCallback& encode_cb);
  ~DirectiveHandler();

  // Adds a directive to handle.
  void AddDirective(const copresence::Directive& directive);
  // Removes any directives associated with the given operation id.
  void RemoveDirectives(const std::string& op_id);

 private:
  scoped_ptr<AudioDirectiveHandler> audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_
