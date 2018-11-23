// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {

Selector::Selector() = default;
Selector::Selector(std::vector<std::string> s) : selectors(s) {}
Selector::~Selector() = default;

Selector::Selector(Selector&& other) = default;
Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector& Selector::operator=(Selector&& other) = default;

bool Selector::operator<(const Selector& other) const {
  return this->selectors < other.selectors;
}

bool Selector::operator==(const Selector& other) const {
  return this->selectors == other.selectors;
}

bool Selector::empty() const {
  return this->selectors.empty();
}

}  // namespace autofill_assistant