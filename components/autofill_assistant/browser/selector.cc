// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/strings/string_util.h"

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

std::ostream& operator<<(std::ostream& out, PseudoType pseudo_type) {
#ifdef NDEBUG
  return out << static_cast<int>(pseudo_type);
#else
  switch (pseudo_type) {
    case UNDEFINED:
      out << "UNDEFINED";
      break;
    case FIRST_LINE:
      out << "FIRST_LINE";
      break;
    case FIRST_LETTER:
      out << "FIRST_LETTER";
      break;
    case BEFORE:
      out << "BEFORE";
      break;
    case AFTER:
      out << "AFTER";
      break;
    case BACKDROP:
      out << "BACKDROP";
      break;
    case SELECTION:
      out << "SELECTION";
      break;
    case FIRST_LINE_INHERITED:
      out << "FIRST_LINE_INHERITED";
      break;
    case SCROLLBAR:
      out << "SCROLLBAR";
      break;
    case SCROLLBAR_THUMB:
      out << "SCROLLBAR_THUMB";
      break;
    case SCROLLBAR_BUTTON:
      out << "SCROLLBAR_BUTTON";
      break;
    case SCROLLBAR_TRACK:
      out << "SCROLLBAR_TRACK";
      break;
    case SCROLLBAR_TRACK_PIECE:
      out << "SCROLLBAR_TRACK_PIECE";
      break;
    case SCROLLBAR_CORNER:
      out << "SCROLLBAR_CORNER";
      break;
    case RESIZER:
      out << "RESIZER";
      break;
    case INPUT_LIST_BUTTON:
      out << "INPUT_LIST_BUTTON";
      break;

      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif
}
std::ostream& operator<<(std::ostream& out, const Selector& selector) {
#ifdef NDEBUG
  out << selector.selectors.size() << " element(s)";
  return out;
#else
  out << "elements=[" << base::JoinString(selector.selectors, ",") << "]";
  if (selector.pseudo_type != PseudoType::UNDEFINED) {
    out << "::" << selector.pseudo_type;
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
