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
#include "components/copresence/public/copresence_constants.h"

namespace copresence {

class AudioDirectiveHandler;
class Directive;
class WhispernetClient;

// The directive handler manages transmit and receive directives.
// TODO(ckehoe): Turn this into an interface.
class DirectiveHandler {
 public:
  explicit DirectiveHandler(scoped_ptr<AudioDirectiveHandler> audio_handler =
      make_scoped_ptr(new AudioDirectiveHandlerImpl));
  virtual ~DirectiveHandler();

  // Starts processing directives with the provided Whispernet client.
  // Directives will be queued until this function is called.
  // |whispernet_client| is owned by the caller and must outlive the
  // DirectiveHandler.
  // |tokens_cb| is called for all audio tokens found in recorded audio.
  virtual void Start(WhispernetClient* whispernet_client,
                     const TokensCallback& tokens_cb);

  // Adds a directive to handle.
  virtual void AddDirective(const Directive& directive);

  // Removes any directives associated with the given operation id.
  virtual void RemoveDirectives(const std::string& op_id);

  // TODO(rkc): Too many audio specific functions here, find a better way to
  // get this information to the copresence manager.
  virtual const std::string GetCurrentAudioToken(AudioType type) const;
  virtual bool IsAudioTokenHeard(AudioType type) const;

 private:
  // Starts actually running a directive.
  void StartDirective(const std::string& op_id, const Directive& directive);

  scoped_ptr<AudioDirectiveHandler> audio_handler_;
  std::map<std::string, std::vector<Directive>> pending_directives_;

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
