// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestion.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"

namespace {

// dict.Get() specialization for base::Time values
bool GetTimeValue(const base::DictionaryValue& dict,
                  const std::string& key,
                  base::Time* time) {
  // TODO(gaschler): Replace all usages of GetString(key, &str) by
  // FindKey(key)->GetString().
  std::string time_value;
  return dict.GetString(key, &time_value) &&
         base::Time::FromString(time_value.c_str(), time);
}

// dict.Get() specialization for GURL values
bool GetURLValue(const base::DictionaryValue& dict,
                 const std::string& key,
                 GURL* url) {
  std::string spec;
  if (!dict.GetString(key, &spec)) {
    return false;
  }
  *url = GURL(spec);
  return url->is_valid();
}

}  // namespace

namespace ntp_snippets {

ContextualSuggestion::ContextualSuggestion(const std::string& id) : id_(id) {}

ContextualSuggestion::~ContextualSuggestion() = default;

// static
std::unique_ptr<ContextualSuggestion>
ContextualSuggestion::CreateFromDictionary(const base::DictionaryValue& dict) {
  std::string id;
  if (!dict.GetString("url", &id) || id.empty()) {
    return nullptr;
  }
  auto suggestion = MakeUnique(id);
  GetURLValue(dict, "url", &suggestion->url_);
  if (!dict.GetString("title", &suggestion->title_)) {
    dict.GetString("source", &suggestion->title_);
  }
  dict.GetString("snippet", &suggestion->snippet_);
  GetTimeValue(dict, "creationTime", &suggestion->publish_date_);
  GetURLValue(dict, "imageUrl", &suggestion->salient_image_url_);
  if (!dict.GetString("attribution", &suggestion->publisher_name_)) {
    dict.GetString("source", &suggestion->publisher_name_);
  }
  return suggestion;
}

ContentSuggestion ContextualSuggestion::ToContentSuggestion() const {
  ContentSuggestion suggestion(
      Category::FromKnownCategory(KnownCategories::CONTEXTUAL), id_, url_);
  suggestion.set_title(base::UTF8ToUTF16(title_));
  suggestion.set_snippet_text(base::UTF8ToUTF16(snippet_));
  suggestion.set_publish_date(publish_date_);
  suggestion.set_publisher_name(base::UTF8ToUTF16(publisher_name_));
  return suggestion;
}

// static
std::unique_ptr<ContextualSuggestion> ContextualSuggestion::CreateForTesting(
    const std::string& to_url,
    const std::string& image_url) {
  auto suggestion = MakeUnique({to_url});
  suggestion->url_ = GURL(to_url);
  suggestion->salient_image_url_ = GURL(image_url);
  return suggestion;
}

// static
std::unique_ptr<ContextualSuggestion> ContextualSuggestion::MakeUnique(
    const std::string& id) {
  return base::WrapUnique(new ContextualSuggestion(id));
}

}  // namespace ntp_snippets
