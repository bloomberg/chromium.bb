// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "components/copresence/public/copresence_constants.h"

namespace copresence {

class Directive;
class WhispernetClient;

// The directive handler manages transmit and receive directives.
class DirectiveHandler {
 public:
  DirectiveHandler() {}
  virtual ~DirectiveHandler() {}

  // Starts processing directives with the provided Whispernet client.
  // Directives will be queued until this function is called.
  // |whispernet_client| is owned by the caller and must outlive the
  // DirectiveHandler.
  // |tokens_cb| is called for all audio tokens found in recorded audio.
  virtual void Start(WhispernetClient* whispernet_client,
                     const TokensCallback& tokens_cb) = 0;

  // Adds a directive to handle.
  virtual void AddDirective(const Directive& directive) = 0;

  // Removes any directives associated with the given operation id.
  virtual void RemoveDirectives(const std::string& op_id) = 0;

  // TODO(rkc): Too many audio specific functions here.
  // Find a better way to get this information to the copresence manager.
  virtual const std::string GetCurrentAudioToken(AudioType type) const = 0;
  virtual bool IsAudioTokenHeard(AudioType type) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
