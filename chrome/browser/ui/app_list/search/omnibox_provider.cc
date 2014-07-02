// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_provider.h"

#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "grit/theme_resources.h"
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

class OmniboxResult : public ChromeSearchResult {
 public:
  OmniboxResult(Profile* profile, const AutocompleteMatch& match)
      : profile_(profile),
        match_(match) {
    set_id(match.destination_url.spec());

    // Derive relevance from omnibox relevance and normalize it to [0, 1].
    // The magic number 1500 is the highest score of an omnibox result.
    // See comments in autocomplete_provider.h.
    set_relevance(match.relevance / 1500.0);

    UpdateIcon();
    UpdateTitleAndDetails();
  }
  virtual ~OmniboxResult() {}

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE {
    chrome::NavigateParams params(profile_,
                                  match_.destination_url,
                                  match_.transition);
    params.disposition = ui::DispositionFromEventFlags(event_flags);
    chrome::Navigate(&params);
  }

  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE {}

  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE {
    return scoped_ptr<ChromeSearchResult>(
        new OmniboxResult(profile_, match_)).Pass();
  }

  virtual ChromeSearchResultType GetType() OVERRIDE {
    return OMNIBOX_SEARCH_RESULT;
  }

 private:
  void UpdateIcon() {
    int resource_id = match_.starred ?
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
  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResult);
};

}  // namespace

OmniboxProvider::OmniboxProvider(Profile* profile)
    : profile_(profile),
      controller_(new AutocompleteController(
          profile,
          TemplateURLServiceFactory::GetForProfile(profile),
          this,
          AutocompleteClassifier::kDefaultOmniboxProviders &
              ~AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
  controller_->search_provider()->set_in_app_list();
}

OmniboxProvider::~OmniboxProvider() {}

void OmniboxProvider::Start(const base::string16& query) {
  controller_->Start(AutocompleteInput(
      query, base::string16::npos, base::string16(), GURL(),
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

    Add(scoped_ptr<SearchResult>(new OmniboxResult(profile_, *it)));
  }
}

void OmniboxProvider::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = controller_->result();
  PopulateFromACResult(result);
}

}  // namespace app_list
