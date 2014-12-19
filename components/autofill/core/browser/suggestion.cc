// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestion.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"

namespace autofill {

SuggestionBackendID::SuggestionBackendID()
    : variant(0) {
}

SuggestionBackendID::SuggestionBackendID(const std::string& g, size_t v)
    : guid(g),
      variant(v) {
}

SuggestionBackendID::~SuggestionBackendID() {
}

bool SuggestionBackendID::operator<(const SuggestionBackendID& other) const {
  if (variant != other.variant)
    return variant < other.variant;
  return guid < other.guid;
}

Suggestion::Suggestion()
    : frontend_id(0) {
}

Suggestion::Suggestion(const Suggestion& other)
    : backend_id(other.backend_id),
      frontend_id(other.frontend_id),
      value(other.value),
      label(other.label),
      icon(other.icon) {
}

Suggestion::Suggestion(const base::string16& v)
    : frontend_id(0),
      value(v) {
}

Suggestion::Suggestion(const std::string& v,
                       const std::string& l,
                       const std::string& i,
                       int fid)
    : frontend_id(fid),
      value(base::UTF8ToUTF16(v)),
      label(base::UTF8ToUTF16(l)),
      icon(base::UTF8ToUTF16(i)) {
}

Suggestion::~Suggestion() {
}

}  // namespace autofill
