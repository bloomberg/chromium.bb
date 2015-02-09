// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using bookmarks::BookmarkModel;

namespace app_list {

namespace {

// The Omnibox keyword for Google search. This is correct even if the user is
// using an international domain (such as google.com.au).
const char kGoogleSearchKeyword[] = "google.com";

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= SearchResult::Tag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= SearchResult::Tag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= SearchResult::Tag::DIM;

  return tag_styles;
}

// Translates ACMatchClassifications into SearchResult tags.
void ACMatchClassificationsToTags(
    const base::string16& text,
    const ACMatchClassifications& text_classes,
    SearchResult::Tags* tags) {
  int tag_styles = SearchResult::Tag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != SearchResult::Tag::NONE) {
      tags->push_back(SearchResult::Tag(
          tag_styles, tag_start, text_class.offset));
      tag_styles = SearchResult::Tag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != SearchResult::Tag::NONE) {
    tags->push_back(SearchResult::Tag(
        tag_styles, tag_start, text.length()));
  }
}

// Converts a Google Search URL into a spoken feedback URL, by adding query
// parameters. |search_url| must be a Google Search URL.
GURL MakeGoogleSearchSpokenFeedbackUrl(const GURL& search_url) {
  std::string query = search_url.query();
  query += "&gs_ivs=1";
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return search_url.ReplaceComponents(replacements);
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             AutocompleteController* autocomplete_controller,
                             bool is_voice_query,
                             const AutocompleteMatch& match)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      is_voice_query_(is_voice_query),
      match_(match) {
  if (match_.search_terms_args && autocomplete_controller_) {
    match_.search_terms_args->from_app_list = true;
    autocomplete_controller_->UpdateMatchDestinationURL(
        *match_.search_terms_args, &match_);
  }
  set_id(match_.destination_url.spec());

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  // The magic number 1500 is the highest score of an omnibox result.
  // See comments in autocomplete_provider.h.
  set_relevance(match_.relevance / 1500.0);

  UpdateIcon();
  UpdateTitleAndDetails();

  // The raw "what you typed" search results should be promoted and
  // automatically selected by voice queries. If a "history" result exactly
  // matches what you typed, then the omnibox will not produce a "what you
  // typed" result; therefore, we must also flag "history" results as voice
  // results if they exactly match the query.
  if (match_.type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
      (match_.type == AutocompleteMatchType::SEARCH_HISTORY &&
       match_.search_terms_args &&
       match_.contents == match_.search_terms_args->original_query)) {
    set_voice_result(true);
  }
}

OmniboxResult::~OmniboxResult() {
}

void OmniboxResult::Open(int event_flags) {
  RecordHistogram(OMNIBOX_SEARCH_RESULT);
  GURL url = match_.destination_url;
  if (is_voice_query_ &&
      base::UTF16ToUTF8(match_.keyword) == kGoogleSearchKeyword) {
    url = MakeGoogleSearchSpokenFeedbackUrl(url);
  }
  list_controller_->OpenURL(profile_, url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

scoped_ptr<SearchResult> OmniboxResult::Duplicate() const {
  return scoped_ptr<SearchResult>(new OmniboxResult(profile_, list_controller_,
                                                    autocomplete_controller_,
                                                    is_voice_query_, match_));
}

void OmniboxResult::UpdateIcon() {
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match_.destination_url);
  int resource_id = is_bookmarked ? IDR_OMNIBOX_STAR
                                  : AutocompleteMatch::TypeToIcon(match_.type);
  SetIcon(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id));
}

void OmniboxResult::UpdateTitleAndDetails() {
  set_title(match_.contents);
  SearchResult::Tags title_tags;
  ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                               &title_tags);
  set_title_tags(title_tags);

  set_details(match_.description);
  SearchResult::Tags details_tags;
  ACMatchClassificationsToTags(match_.description, match_.description_class,
                               &details_tags);
  set_details_tags(details_tags);
}

}  // namespace app_list
