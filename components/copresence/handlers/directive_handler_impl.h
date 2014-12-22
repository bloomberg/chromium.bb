// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_

#include "components/copresence/handlers/directive_handler.h"

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"

namespace copresence {

// The directive handler manages transmit and receive directives.
class DirectiveHandlerImpl final : public DirectiveHandler {
 public:
  explicit DirectiveHandlerImpl(scoped_ptr<AudioDirectiveHandler>
      audio_handler = make_scoped_ptr(new AudioDirectiveHandlerImpl));
  ~DirectiveHandlerImpl() override;

  // DirectiveHandler overrides
  void Start(WhispernetClient* whispernet_client,
             const TokensCallback& tokens_cb) override;
  void AddDirective(const Directive& directive) override;
  void RemoveDirectives(const std::string& op_id) override;
  const std::string GetCurrentAudioToken(AudioType type) const override;
  bool IsAudioTokenHeard(AudioType type) const override;

 private:
  // Starts actually running a directive.
  void StartDirective(const std::string& op_id, const Directive& directive);

  scoped_ptr<AudioDirectiveHandler> audio_handler_;
  std::map<std::string, std::vector<Directive>> pending_directives_;

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveHandlerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_
