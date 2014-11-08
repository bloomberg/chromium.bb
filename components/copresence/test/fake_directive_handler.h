// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_TEST_FAKE_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_TEST_FAKE_DIRECTIVE_HANDLER_H_

#include <string>
#include <vector>

#include "components/copresence/handlers/directive_handler.h"

namespace copresence {

// TODO(ckehoe): Make DirectiveHandler an interface.
class FakeDirectiveHandler final : public DirectiveHandler {
 public:
  FakeDirectiveHandler();
  ~FakeDirectiveHandler() override;

  const std::vector<std::string>& added_directives() const {
    return added_directives_;
  }

  const std::vector<std::string>& removed_directives() const {
    return removed_directives_;
  }

  void Start(WhispernetClient* /* whispernet_client */,
             const TokensCallback& /* tokens_cb */) override {}

  void AddDirective(const Directive& directive) override;

  void RemoveDirectives(const std::string& op_id) override;

  const std::string GetCurrentAudioToken(AudioType type) const override;

  bool IsAudioTokenHeard(AudioType type) const override;

 private:
  std::vector<std::string> added_directives_;
  std::vector<std::string> removed_directives_;

  DISALLOW_COPY_AND_ASSIGN(FakeDirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_TEST_FAKE_DIRECTIVE_HANDLER_H_
