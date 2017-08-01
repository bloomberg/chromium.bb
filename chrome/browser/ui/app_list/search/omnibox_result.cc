// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include <stddef.h>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using bookmarks::BookmarkModel;

namespace app_list {

namespace {

// #000 at 87% opacity.
constexpr SkColor kListIconColor = SkColorSetARGBMacro(0xDE, 0x00, 0x00, 0x00);

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
void ACMatchClassificationsToTags(const base::string16& text,
                                  const ACMatchClassifications& text_classes,
                                  SearchResult::Tags* tags) {
  int tag_styles = SearchResult::Tag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != SearchResult::Tag::NONE) {
      tags->push_back(
          SearchResult::Tag(tag_styles, tag_start, text_class.offset));
      tag_styles = SearchResult::Tag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != SearchResult::Tag::NONE) {
    tags->push_back(SearchResult::Tag(tag_styles, tag_start, text.length()));
  }
}

// Returns true if |url| is on a Google Search domain. May return false
// positives.
bool IsUrlGoogleSearch(const GURL& url) {
  // Just return true if the second or third level domain is "google". This may
  // result in false positives (e.g. "google.example.com"), but since we are
  // only using this to decide when to add the spoken feedback query parameter,
  // this doesn't have any bad consequences.
  const char kGoogleDomainLabel[] = "google";

  std::vector<std::string> pieces = base::SplitString(
      url.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  size_t num_pieces = pieces.size();

  if (num_pieces >= 2 && pieces[num_pieces - 2] == kGoogleDomainLabel)
    return true;

  if (num_pieces >= 3 && pieces[num_pieces - 3] == kGoogleDomainLabel)
    return true;

  return false;
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

// AutocompleteMatchType::Type to vector icon, used for app list.
const gfx::VectorIcon& TypeToVectorIcon(AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::CLIPBOARD:
    case AutocompleteMatchType::PHYSICAL_WEB:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW:
      return kIcDomainIcon;

    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::VOICE_SUGGEST:
      return kIcSearchIcon;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return kIcHistoryIcon;

    case AutocompleteMatchType::CALCULATOR:
      return kIcEqualIcon;

    case AutocompleteMatchType::EXTENSION_APP:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return kIcDomainIcon;
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
  set_comparable_id(match_.stripped_destination_url.spec());

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

OmniboxResult::~OmniboxResult() = default;

void OmniboxResult::Open(int event_flags) {
  RecordHistogram(OMNIBOX_SEARCH_RESULT);
  GURL url = match_.destination_url;
  if (is_voice_query_ && IsUrlGoogleSearch(url)) {
    url = MakeGoogleSearchSpokenFeedbackUrl(url);
  }
  list_controller_->OpenURL(profile_, url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

std::unique_ptr<SearchResult> OmniboxResult::Duplicate() const {
  return std::unique_ptr<SearchResult>(
      new OmniboxResult(profile_, list_controller_, autocomplete_controller_,
                        is_voice_query_, match_));
}

void OmniboxResult::UpdateIcon() {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match_.destination_url);

  if (features::IsFullscreenAppListEnabled()) {
    const gfx::VectorIcon& icon =
        is_bookmarked ? kIcBookmarkIcon : TypeToVectorIcon(match_.type);
    SetIcon(
        gfx::CreateVectorIcon(icon, kListIconSizeFullscreen, kListIconColor));
  } else {
    const gfx::VectorIcon& icon =
        is_bookmarked ? omnibox::kStarIcon
                      : AutocompleteMatch::TypeToVectorIcon(match_.type);
    SetIcon(gfx::CreateVectorIcon(icon, 16, kIconColor));
  }
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
