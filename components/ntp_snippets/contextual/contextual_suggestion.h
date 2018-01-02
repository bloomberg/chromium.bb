// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTION_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ntp_snippets {

class ContextualSuggestion {
 public:
  using PtrVector = std::vector<std::unique_ptr<ContextualSuggestion>>;

  ~ContextualSuggestion();

  // Creates a ContextualSuggestion from a dictionary. Returns a null pointer if
  // the dictionary doesn't correspond to a valid suggestion.
  static std::unique_ptr<ContextualSuggestion> CreateFromDictionary(
      const base::DictionaryValue& dict);

  // Converts to general content suggestion form.
  ContentSuggestion ToContentSuggestion() const;

  // The ID for identifying the suggestion.
  const std::string& id() const { return id_; }

  // Title of the suggestion.
  const std::string& title() const { return title_; }

  // The main URL pointing to the content web page.
  const GURL& url() const { return url_; }

  // The name of the content's publisher.
  const std::string& publisher_name() const { return publisher_name_; }

  // Summary or relevant extract from the content.
  const std::string& snippet() const { return snippet_; }

  // Link to an image representative of the content. Do not fetch directly,
  // but through the service, which uses caching, to avoid multiple
  // network requests.
  const GURL& salient_image_url() const { return salient_image_url_; }

  static std::unique_ptr<ContextualSuggestion> CreateForTesting(
      const std::string& to_url,
      const std::string& image_url);

 private:
  ContextualSuggestion(const std::string& id);

  // std::make_unique doesn't work if the ctor is private.
  static std::unique_ptr<ContextualSuggestion> MakeUnique(
      const std::string& id);

  std::string id_;
  std::string title_;
  GURL url_;
  std::string publisher_name_;
  GURL salient_image_url_;
  std::string snippet_;
  base::Time publish_date_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTION_H_
