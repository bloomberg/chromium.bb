// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/search_builder.h"

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/ash/extension_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= app_list::SearchResult::Tag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= app_list::SearchResult::Tag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= app_list::SearchResult::Tag::DIM;

  return tag_styles;
}

// Translates ACMatchClassifications into SearchResult tags.
void ACMatchClassificationsToTags(
    const string16& text,
    const ACMatchClassifications& text_classes,
    app_list::SearchResult::Tags* tags) {
  int tag_styles = app_list::SearchResult::Tag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != app_list::SearchResult::Tag::NONE) {
      tags->push_back(app_list::SearchResult::Tag(
          tag_styles, tag_start, text_class.offset));
      tag_styles = app_list::SearchResult::Tag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != app_list::SearchResult::Tag::NONE) {
    tags->push_back(app_list::SearchResult::Tag(
        tag_styles, tag_start, text.length()));
  }
}

// SearchBuildResult is an app list SearchResult built from an
// AutocompleteMatch.
class SearchBuilderResult : public app_list::SearchResult {
 public:
  SearchBuilderResult(Profile* profile,
                      const AutocompleteMatch& match)
      : profile_(profile),
        match_(match) {
    UpdateIcon();
    UpdateTitleAndDetails();
  }

  const AutocompleteMatch& match() const {
    return match_;
  }

 private:
  void UpdateIcon() {
    const TemplateURL* template_url = match_.GetTemplateURL(profile_);
    if (template_url && template_url->IsExtensionKeyword()) {
      set_icon(profile_->GetExtensionService()->GetOmniboxPopupIcon(
          template_url->GetExtensionId()));
      return;
    }

    int resource_id = match_.starred ?
        IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
    set_icon(*ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
        resource_id));
  }

  void UpdateTitleAndDetails() {
    set_title(match_.contents);
    app_list::SearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents,
                                 match_.contents_class,
                                 &title_tags);
    set_title_tags(title_tags);

    set_details(match_.description);
    app_list::SearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.description,
                                 match_.description_class,
                                 &details_tags);
    set_details_tags(details_tags);
  }

  Profile* profile_;
  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(SearchBuilderResult);
};

}  // namespace

SearchBuilder::SearchBuilder(
    Profile* profile,
    app_list::SearchBoxModel* search_box,
    app_list::AppListModel::SearchResults* results)
    : profile_(profile),
      search_box_(search_box),
      results_(results),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          controller_(new AutocompleteController(profile, this))) {
  search_box_->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));
  search_box_->SetIcon(*ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_OMNIBOX_SEARCH));
}

SearchBuilder::~SearchBuilder() {
}

void SearchBuilder::StartSearch() {
  string16 user_text = search_box_->text();
  string16 empty_string;

  // Omnibox features such as keyword selection/accepting and instant query
  // are not implemented.
  // TODO(xiyuan): Figure out the features that need to support here.
  controller_->Start(user_text,
                     empty_string,  // desired TLD.
                     false,  // don't prevent inline autocompletion
                     false,  // no preferred keyword provider
                     true,  // allow exact keyword matches
                     AutocompleteInput::ALL_MATCHES);
}

void SearchBuilder::StopSearch() {
  controller_->Stop(true /* clear_result */);
}

void SearchBuilder::OpenResult(const app_list::SearchResult& result,
                               int event_flags) {
  const SearchBuilderResult* builder_result =
      static_cast<const SearchBuilderResult*>(&result);
  const AutocompleteMatch& match = builder_result->match();

  if (match.type == AutocompleteMatch::EXTENSION_APP) {
    ExtensionService* service = profile_->GetExtensionService();
    const extensions::Extension* extension =
        service->GetInstalledApp(match.destination_url);
    if (extension)
      extension_utils::OpenExtension(profile_, extension, event_flags);
  } else {
    WindowOpenDisposition disposition =
        browser::DispositionFromEventFlags(event_flags);
    Browser* browser = browser::FindOrCreateTabbedBrowser(profile_);

    if (disposition == CURRENT_TAB) {
      // If current tab is not NTP, change disposition to NEW_FOREGROUND_TAB.
      const GURL& url = browser->GetSelectedWebContents() ?
          browser->GetSelectedWebContents()->GetURL() : GURL();
      if (!url.SchemeIs(chrome::kChromeUIScheme) ||
          url.host() != chrome::kChromeUINewTabHost) {
        disposition = NEW_FOREGROUND_TAB;
      }
    }

    // TODO(xiyuan): What should we do for alternate url case?
    browser->OpenURL(
        content::OpenURLParams(match.destination_url,
                               content::Referrer(),
                               disposition,
                               match.transition,
                               false));
  }
}

void SearchBuilder::OnResultChanged(bool default_match_changed) {
  // TODO(xiyuan): Handle default match properly.
  const AutocompleteResult& ac_result = controller_->result();
  results_->DeleteAll();
  for (ACMatches::const_iterator it = ac_result.begin();
       it != ac_result.end();
       ++it) {
    results_->Add(new SearchBuilderResult(profile_, *it));
  }
}
