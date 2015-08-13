// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_

#include "base/strings/string16.h"
#include "components/metrics/proto/omnibox_event.pb.h"

struct AutocompleteMatch;
class AutocompleteProviderClient;

// Returns a verbatim match for |input_text| with |classification| and a
// relevance of |verbatim_relevance|. If |verbatim_relevance| is negative
// then a default value is used.
AutocompleteMatch VerbatimMatchForURL(
    AutocompleteProviderClient* client,
    const base::string16& input_text,
    metrics::OmniboxEventProto::PageClassification classification,
    int verbatim_relevance);

#endif  // COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_
