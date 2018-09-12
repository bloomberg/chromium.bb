// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_memory.h"

namespace autofill_assistant {

ClientMemory::ClientMemory() = default;
ClientMemory::~ClientMemory() = default;

base::Optional<std::string> ClientMemory::selected_card() {
  return selected_card_;
}

base::Optional<std::string> ClientMemory::selected_address(
    const std::string& name) {
  if (selected_addresses_.find(name) != selected_addresses_.end())
    return selected_addresses_[name];

  return base::nullopt;
}

void ClientMemory::set_selected_card(const std::string& guid) {
  selected_card_ = guid;
}

void ClientMemory::set_selected_address(const std::string& name,
                                        const std::string& guid) {
  selected_addresses_[name] = guid;
}

}  // namespace autofill_assistant
