// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
#define CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_

#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace vr {

struct OmniboxSuggestion {
  OmniboxSuggestion(const base::string16& new_content,
                    const base::string16& new_description,
                    AutocompleteMatch::Type new_type);

  base::string16 content;
  base::string16 description;
  AutocompleteMatch::Type type;
};

struct OmniboxSuggestions {
  OmniboxSuggestions();
  ~OmniboxSuggestions();

  std::vector<OmniboxSuggestion> suggestions;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
