// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestion.h"

namespace ntp_snippets {

ContentSuggestion::ContentSuggestion(const std::string& id, const GURL& url)
    : id_(id), url_(url), score_(0) {}

ContentSuggestion::ContentSuggestion(ContentSuggestion&&) = default;

ContentSuggestion& ContentSuggestion::operator=(ContentSuggestion&&) = default;

ContentSuggestion::~ContentSuggestion() {}

}  // namespace ntp_snippets
