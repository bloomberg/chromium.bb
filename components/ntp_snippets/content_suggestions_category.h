// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_H_

#include <ostream>

namespace ntp_snippets {

class ContentSuggestionsCategoryFactory;

// These are the categories that the client knows about.
// The values before LOCAL_CATEGORIES_COUNT are the categories that are provided
// locally on the device. Those values need to be listed in the
// ContentSuggestionsCategoryFactory constructor!
// Categories provided by the server (IDs strictly larger than
// REMOTE_CATEGORIES_OFFSET) only need to be hard-coded here if they need to be
// recognized by the client implementation.
enum class KnownSuggestionsCategories {
  OFFLINE_PAGES,
  LOCAL_CATEGORIES_COUNT,

  REMOTE_CATEGORIES_OFFSET = 10000,
  ARTICLES = REMOTE_CATEGORIES_OFFSET + 1,
};

// A category groups ContentSuggestions which belong together. Use the
// ContentSuggestionsCategoryFactory to obtain instances.
class ContentSuggestionsCategory {
 public:
  // Returns a non-negative identifier that is unique for the category and can
  // be converted back to a ContentSuggestionsCategory instance using
  // |ContentSuggestionsCategoryFactory::FromIDValue(id)|.
  // Note that these IDs are not necessarily stable across multiple runs of
  // the application, so they should not be persisted.
  int id() const { return id_; }

  bool IsKnownCategory(KnownSuggestionsCategories known_category) const;

 private:
  friend class ContentSuggestionsCategoryFactory;

  ContentSuggestionsCategory(int id);

  int id_;

  // Allow copy and assignment.
};

bool operator==(const ContentSuggestionsCategory& left,
                const ContentSuggestionsCategory& right);

bool operator!=(const ContentSuggestionsCategory& left,
                const ContentSuggestionsCategory& right);

std::ostream& operator<<(std::ostream& os,
                         const ContentSuggestionsCategory& obj);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_H_
