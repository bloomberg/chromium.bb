// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_provider.h"

#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "grit/theme_resources.h"
#include "ui/app_list/search_result.h"
#include "ui/base/resource/resource_bundle.h"

namespace app_list {

namespace {

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

class OmniboxResult : public SearchResult {
 public:
  OmniboxResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                AutocompleteController* autocomplete_controller,
                const AutocompleteMatch& match)
      : profile_(profile),
        list_controller_(list_controller),
        autocomplete_controller_(autocomplete_controller),
        match_(match) {
    if (match_.search_terms_args) {
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
         match_.contents == match_.search_terms_args->original_query)) {
      set_voice_result(true);
    }
  }
  ~OmniboxResult() override {}

  // SearchResult overrides:
  void Open(int event_flags) override {
    RecordHistogram(OMNIBOX_SEARCH_RESULT);
    list_controller_->OpenURL(profile_,
                              match_.destination_url,
                              match_.transition,
                              ui::DispositionFromEventFlags(event_flags));
  }

  scoped_ptr<SearchResult> Duplicate() override {
    return scoped_ptr<SearchResult>(new OmniboxResult(
        profile_, list_controller_, autocomplete_controller_, match_));
  }

 private:
  void UpdateIcon() {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(profile_);
    bool is_bookmarked =
        bookmark_model && bookmark_model->IsBookmarked(match_.destination_url);
    int resource_id = is_bookmarked ?
        IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
    SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        resource_id));
  }

  void UpdateTitleAndDetails() {
    set_title(match_.contents);
    SearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents,
                                 match_.contents_class,
                                 &title_tags);
    set_title_tags(title_tags);

    set_details(match_.description);
    SearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.description,
                                 match_.description_class,
                                 &details_tags);
    set_details_tags(details_tags);
  }

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  AutocompleteController* autocomplete_controller_;
  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResult);
};

}  // namespace

OmniboxProvider::OmniboxProvider(Profile* profile,
                                 AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      controller_(new AutocompleteController(
          profile,
          TemplateURLServiceFactory::GetForProfile(profile),
          this,
          AutocompleteClassifier::kDefaultOmniboxProviders &
              ~AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
}

OmniboxProvider::~OmniboxProvider() {}

void OmniboxProvider::Start(const base::string16& query) {
  controller_->Start(AutocompleteInput(
      query, base::string16::npos, std::string(), GURL(),
      metrics::OmniboxEventProto::INVALID_SPEC, false, false, true, true,
      ChromeAutocompleteSchemeClassifier(profile_)));
}

void OmniboxProvider::Stop() {
  controller_->Stop(false);
}

void OmniboxProvider::PopulateFromACResult(const AutocompleteResult& result) {
  ClearResults();
  for (ACMatches::const_iterator it = result.begin();
       it != result.end();
       ++it) {
    if (!it->destination_url.is_valid())
      continue;

    Add(scoped_ptr<SearchResult>(
        new OmniboxResult(profile_, list_controller_, controller_.get(), *it)));
  }
}

void OmniboxProvider::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = controller_->result();
  PopulateFromACResult(result);
}

}  // namespace app_list
