// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

namespace ntp_snippets {

const base::Feature kBookmarkSuggestionsFeature{
    "NTPBookmarkSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsFeature{
    "NTPSnippets", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace ntp_snippets
