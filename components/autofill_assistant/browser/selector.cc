// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {

Selector::Selector() : pseudo_type(PseudoType::UNDEFINED) {}

Selector::Selector(const ElementReferenceProto& element) {
  for (const auto& selector : element.selectors()) {
    selectors.emplace_back(selector);
  }
  pseudo_type = element.pseudo_type();
}

Selector::Selector(std::vector<std::string> s)
    : selectors(s), pseudo_type(PseudoType::UNDEFINED) {}
Selector::Selector(std::vector<std::string> s, PseudoType p)
    : selectors(s), pseudo_type(p) {}
Selector::~Selector() = default;

Selector::Selector(Selector&& other) = default;
Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector& Selector::operator=(Selector&& other) = default;

bool Selector::operator<(const Selector& other) const {
  return this->selectors < other.selectors ||
         (this->selectors == other.selectors &&
          this->pseudo_type < other.pseudo_type);
}

bool Selector::operator==(const Selector& other) const {
  return this->selectors == other.selectors &&
         this->pseudo_type == other.pseudo_type;
}

bool Selector::empty() const {
  return this->selectors.empty();
}

}  // namespace autofill_assistant