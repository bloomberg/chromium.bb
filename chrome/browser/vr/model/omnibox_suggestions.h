// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
#define CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_

#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "url/gurl.h"

namespace vr {

struct OmniboxSuggestion {
  OmniboxSuggestion(const base::string16& new_content,
                    const base::string16& new_description,
                    AutocompleteMatch::Type new_type,
                    GURL new_destination);
  OmniboxSuggestion(const OmniboxSuggestion& other);

  base::string16 content;
  base::string16 description;
  AutocompleteMatch::Type type;
  GURL destination;
};

struct OmniboxSuggestions {
  OmniboxSuggestions();
  ~OmniboxSuggestions();

  std::vector<OmniboxSuggestion> suggestions;
};

// This struct represents the current request to the AutocompleteController.
struct AutocompleteStatus {
  bool active = false;
  base::string16 input;

  bool operator==(const AutocompleteStatus& other) const {
    return active == other.active && input == other.input;
  }
  bool operator!=(const AutocompleteStatus& other) const {
    return !(*this == other);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
