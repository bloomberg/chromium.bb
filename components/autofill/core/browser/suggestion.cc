// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestion.h"

#include "base/strings/utf_string_conversions.h"

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

Suggestion::Suggestion(const base::string16& v)
    : frontend_id(0),
      value(v),
      is_masked(false) {
}

Suggestion::Suggestion(const std::string& v,
                       const std::string& l,
                       const std::string& i,
                       int fid)
    : frontend_id(fid),
      value(base::UTF8ToUTF16(v)),
      label(base::UTF8ToUTF16(l)),
      icon(base::UTF8ToUTF16(i)),
      is_masked(false) {
}

Suggestion::Suggestion()
    : frontend_id(0),
      is_masked(false) {
}

Suggestion::~Suggestion() {
}

}  // namespace autofill
