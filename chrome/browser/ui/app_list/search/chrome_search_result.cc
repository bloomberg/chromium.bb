// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

#include <map>

#include "ash/public/cpp/app_list/tokenized_string.h"
#include "ash/public/cpp/app_list/tokenized_string_match.h"
#include "base/containers/adapters.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

ChromeSearchResult::ChromeSearchResult() = default;

void ChromeSearchResult::UpdateFromMatch(
    const app_list::TokenizedString& title,
    const app_list::TokenizedStringMatch& match) {
  const app_list::TokenizedStringMatch::Hits& hits = match.hits();

  Tags tags;
  tags.reserve(hits.size());
  for (const auto& hit : hits)
    tags.push_back(Tag(Tag::MATCH, hit.start(), hit.end()));

  set_title(title.text());
  set_title_tags(tags);
  set_relevance(match.relevance());
}

void ChromeSearchResult::GetContextMenuModel(GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void ChromeSearchResult::ContextMenuItemSelected(int command_id,
                                                 int event_flags) {
  if (GetAppContextMenu())
    GetAppContextMenu()->ExecuteCommand(command_id, event_flags);
}

// static
std::string ChromeSearchResult::TagsDebugStringForTest(const std::string& text,
                                                       const Tags& tags) {
  std::string result = text;

  // Build a table of delimiters to insert.
  std::map<size_t, std::string> inserts;
  for (const auto& tag : tags) {
    if (tag.styles & Tag::URL)
      inserts[tag.range.start()].push_back('{');
    if (tag.styles & Tag::MATCH)
      inserts[tag.range.start()].push_back('[');
    if (tag.styles & Tag::DIM) {
      inserts[tag.range.start()].push_back('<');
      inserts[tag.range.end()].push_back('>');
    }
    if (tag.styles & Tag::MATCH)
      inserts[tag.range.end()].push_back(']');
    if (tag.styles & Tag::URL)
      inserts[tag.range.end()].push_back('}');
  }
  // Insert the delimiters (in reverse order, to preserve indices).
  for (const auto& insert : base::Reversed(inserts))
    result.insert(insert.first, insert.second);

  return result;
}

app_list::AppContextMenu* ChromeSearchResult::GetAppContextMenu() {
  return nullptr;
}
