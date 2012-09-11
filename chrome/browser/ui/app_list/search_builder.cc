// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search_builder.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/app_list/app_list_switches.h"
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

const extensions::Extension* GetExtensionByURL(Profile* profile,
                                               const GURL& url) {
  ExtensionService* service = profile->GetExtensionService();
  // Need to explicitly get chrome app because it does not override new tab and
  // not having a web extent to include new tab url, thus GetInstalledApp does
  // not find it.
  return url.spec() == chrome::kChromeUINewTabURL ?
      service->extensions()->GetByID(extension_misc::kChromeAppId) :
      service->GetInstalledApp(url);
}

// SearchBuildResult is an app list SearchResult built from an
// AutocompleteMatch.
class SearchBuilderResult : public app_list::SearchResult,
                            public ImageLoadingTracker::Observer {
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
    if (match_.type == AutocompleteMatch::EXTENSION_APP) {
      const extensions::Extension* extension =
          GetExtensionByURL(profile_, match_.destination_url);
      if (extension) {
        LoadExtensionIcon(extension);
        return;
      }
    }

    int resource_id = match_.starred ?
        IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
    SetIcon(*ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
        resource_id));
  }

  void LoadExtensionIcon(const extensions::Extension* extension) {
    tracker_.reset(new ImageLoadingTracker(this));
    // TODO(xiyuan): Fix this for HD.
    tracker_->LoadImage(extension,
                        extension->GetIconResource(
                            extension_misc::EXTENSION_ICON_SMALL,
                            ExtensionIconSet::MATCH_BIGGER),
                        gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                                  extension_misc::EXTENSION_ICON_SMALL),
                        ImageLoadingTracker::DONT_CACHE);
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

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int tracker_index) OVERRIDE {
    if (!image.IsEmpty()) {
      SetIcon(*image.ToSkBitmap());
      return;
    }

    SetIcon(profile_->GetExtensionService()->GetOmniboxPopupIcon(extension_id).
        AsImageSkia());
  }

  Profile* profile_;
  AutocompleteMatch match_;
  scoped_ptr<ImageLoadingTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(SearchBuilderResult);
};

}  // namespace

SearchBuilder::SearchBuilder(
    Profile* profile,
    app_list::SearchBoxModel* search_box,
    app_list::AppListModel::SearchResults* results,
    AppListController* list_controller)
    : profile_(profile),
      search_box_(search_box),
      results_(results),
      list_controller_(list_controller) {
  search_box_->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));
  search_box_->SetIcon(*ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_OMNIBOX_SEARCH));

  // TODO(xiyuan): Consider requesting fewer providers in the non-apps-only
  // case.
  int providers =
      CommandLine::ForCurrentProcess()->HasSwitch(
          app_list::switches::kAppListShowAppsOnly) ?
      AutocompleteProvider::TYPE_EXTENSION_APP :
      AutocompleteClassifier::kDefaultOmniboxProviders;
  controller_.reset(new AutocompleteController(profile, this, providers));
}

SearchBuilder::~SearchBuilder() {
}

void SearchBuilder::StartSearch() {
  // Omnibox features such as keyword selection/accepting and instant query
  // are not implemented.
  // TODO(xiyuan): Figure out the features that need to support here.
  controller_->Start(search_box_->text(), string16(), false, false, true,
      AutocompleteInput::ALL_MATCHES);
}

void SearchBuilder::StopSearch() {
  controller_->Stop(true);
}

void SearchBuilder::OpenResult(const app_list::SearchResult& result,
                               int event_flags) {
  const SearchBuilderResult* builder_result =
      static_cast<const SearchBuilderResult*>(&result);
  const AutocompleteMatch& match = builder_result->match();

  // Count AppList.Search here because it is composed of search + action.
  content::RecordAction(content::UserMetricsAction("AppList_Search"));

  if (match.type == AutocompleteMatch::EXTENSION_APP) {
    const extensions::Extension* extension =
        GetExtensionByURL(profile_, match.destination_url);
    if (extension) {
      content::RecordAction(
          content::UserMetricsAction("AppList_ClickOnAppFromSearch"));
      list_controller_->ActivateApp(profile_, extension->id(), event_flags);
    }
  } else {
    // TODO(xiyuan): What should we do for alternate url case?
    chrome::NavigateParams params(profile_,
                                  match.destination_url,
                                  match.transition);
    params.disposition = chrome::DispositionFromEventFlags(event_flags);
    chrome::Navigate(&params);
  }
}

void SearchBuilder::InvokeResultAction(const app_list::SearchResult& result,
                                       int action_index,
                                       int event_flags) {
  NOTIMPLEMENTED();
}

void SearchBuilder::PopulateFromACResult(const AutocompleteResult& ac_result) {
  results_->DeleteAll();
  for (ACMatches::const_iterator it = ac_result.begin();
       it != ac_result.end();
       ++it) {
    results_->Add(new SearchBuilderResult(profile_, *it));
  }
}

void SearchBuilder::OnResultChanged(bool default_match_changed) {
  // TODO(xiyuan): Handle default match properly.
  const AutocompleteResult& ac_result = controller_->result();
  PopulateFromACResult(ac_result);
}
