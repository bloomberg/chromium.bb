// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"
#include "components/copresence/public/whispernet_client.h"

namespace copresence {

class AudioDirectiveHandler;
class Directive;

// The directive handler manages transmit and receive directives.
// TODO(ckehoe): Turn this into an interface.
class DirectiveHandler {
 public:
  explicit DirectiveHandler(scoped_ptr<AudioDirectiveHandler> audio_handler =
      make_scoped_ptr(new AudioDirectiveHandlerImpl));
  virtual ~DirectiveHandler();

  // Starts processing directives with the provided Whispernet client.
  // Directives will be queued until this function is called.
  // |whispernet_client| is owned by the caller
  // and must outlive the DirectiveHandler.
  virtual void Start(WhispernetClient* whispernet_client);

  // Adds a directive to handle.
  virtual void AddDirective(const Directive& directive);

  // Removes any directives associated with the given operation id.
  virtual void RemoveDirectives(const std::string& op_id);

  virtual const std::string GetCurrentAudioToken(AudioType type) const;

 private:
  // Starts actually running a directive.
  void StartDirective(const std::string& op_id, const Directive& directive);

  // Forwards the request to encode a token to whispernet,
  // and instructs it to call samples_callback when encoding is complete.
  void EncodeToken(
      const std::string& token,
      AudioType type,
      const WhispernetClient::SamplesCallback& samples_callback);

  scoped_ptr<AudioDirectiveHandler> audio_handler_;
  std::map<std::string, std::vector<Directive>> pending_directives_;

  // Belongs to the caller.
  WhispernetClient* whispernet_client_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
