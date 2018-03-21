// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_result.h"

#include <map>

#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/public/cpp/app_list/tokenized_string.h"
#include "ash/public/cpp/app_list/tokenized_string_match.h"

namespace app_list {

SearchResult::SearchResult() = default;

SearchResult::~SearchResult() {
  for (auto& observer : observers_)
    observer.OnResultDestroying();
}

void SearchResult::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  for (auto& observer : observers_)
    observer.OnIconChanged();
}

void SearchResult::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  badge_icon_ = badge_icon;
  for (auto& observer : observers_)
    observer.OnBadgeIconChanged();
}

void SearchResult::SetRating(float rating) {
  rating_ = rating;
  for (auto& observer : observers_)
    observer.OnRatingChanged();
}

void SearchResult::SetFormattedPrice(const base::string16& formatted_price) {
  formatted_price_ = formatted_price;
  for (auto& observer : observers_)
    observer.OnFormattedPriceChanged();
}

void SearchResult::SetActions(const Actions& sets) {
  actions_ = sets;
  for (auto& observer : observers_)
    observer.OnActionsChanged();
}

void SearchResult::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  for (auto& observer : observers_)
    observer.OnIsInstallingChanged();
}

void SearchResult::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  for (auto& observer : observers_)
    observer.OnPercentDownloadedChanged();
}

void SearchResult::NotifyItemInstalled() {
  for (auto& observer : observers_)
    observer.OnItemInstalled();
}

void SearchResult::AddObserver(SearchResultObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchResult::RemoveObserver(SearchResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchResult::UpdateFromMatch(const TokenizedString& title,
                                   const TokenizedStringMatch& match) {
  const TokenizedStringMatch::Hits& hits = match.hits();

  Tags tags;
  tags.reserve(hits.size());
  for (size_t i = 0; i < hits.size(); ++i)
    tags.push_back(Tag(Tag::MATCH, hits[i].start(), hits[i].end()));

  set_title(title.text());
  set_title_tags(tags);
  set_relevance(match.relevance());
}

void SearchResult::Open(int event_flags) {}

void SearchResult::InvokeAction(int action_index, int event_flags) {}

ui::MenuModel* SearchResult::GetContextMenuModel() {
  return NULL;
}

// static
std::string SearchResult::TagsDebugString(const std::string& text,
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
  for (auto it = inserts.rbegin(); it != inserts.rend(); ++it)
    result.insert(it->first, it->second);

  return result;
}

}  // namespace app_list
