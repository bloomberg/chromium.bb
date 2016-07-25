// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_category_status.h"

namespace ntp_snippets {

bool IsContentSuggestionsCategoryStatusAvailable(
    ContentSuggestionsCategoryStatus status) {
  // Note: This code is duplicated in SnippetsBridge.java.
  return status == ContentSuggestionsCategoryStatus::AVAILABLE_LOADING ||
         status == ContentSuggestionsCategoryStatus::AVAILABLE;
}

bool IsContentSuggestionsCategoryStatusInitOrAvailable(
    ContentSuggestionsCategoryStatus status) {
  // Note: This code is duplicated in SnippetsBridge.java.
  return status == ContentSuggestionsCategoryStatus::INITIALIZING ||
         IsContentSuggestionsCategoryStatusAvailable(status);
}

}  // namespace ntp_snippets
