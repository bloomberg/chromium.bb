// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestion.h"

#include "base/strings/utf_string_conversions.h"

namespace autofill {

Suggestion::Suggestion()
    : frontend_id(0), match(PREFIX_MATCH), is_value_secondary(false) {}

Suggestion::Suggestion(const Suggestion& other)
    : backend_id(other.backend_id),
      frontend_id(other.frontend_id),
      value(other.value),
      label(other.label),
      additional_label(other.additional_label),
      custom_icon(other.custom_icon),
      icon(other.icon),
      match(other.match),
      is_value_secondary(other.is_value_secondary) {}

Suggestion::Suggestion(const base::string16& v)
    : frontend_id(0),
      value(v),
      match(PREFIX_MATCH),
      is_value_secondary(false) {}

Suggestion::Suggestion(const std::string& v,
                       const std::string& l,
                       const std::string& i,
                       int fid)
    : frontend_id(fid),
      value(base::UTF8ToUTF16(v)),
      label(base::UTF8ToUTF16(l)),
      icon(i),
      match(PREFIX_MATCH),
      is_value_secondary(false) {}

Suggestion::~Suggestion() = default;

}  // namespace autofill
