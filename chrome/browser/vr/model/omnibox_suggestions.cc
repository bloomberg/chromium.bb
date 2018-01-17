// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/omnibox_suggestions.h"

namespace vr {

OmniboxSuggestion::OmniboxSuggestion(const base::string16& new_content,
                                     const base::string16& new_description,
                                     AutocompleteMatch::Type new_type,
                                     GURL new_destination)
    : content(new_content),
      description(new_description),
      type(new_type),
      destination(new_destination) {}

OmniboxSuggestion::OmniboxSuggestion(const OmniboxSuggestion& other) {
  content = other.content;
  description = other.description;
  type = other.type;
  destination = other.destination;
}

OmniboxSuggestions::OmniboxSuggestions() {}

OmniboxSuggestions::~OmniboxSuggestions() {}

}  // namespace vr
