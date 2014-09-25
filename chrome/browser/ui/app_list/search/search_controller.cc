// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/history.h"
#include "chrome/browser/ui/app_list/search/history_factory.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/people/people_provider.h"
#include "chrome/browser/ui/app_list/search/webstore/webstore_provider.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
  const char kAppListSearchResultOpenTypeHistogram[] =
      "Apps.AppListSearchResultOpenType";

  // Maximum time (in milliseconds) to wait to the search providers to finish.
  const int kStopTimeMS = 1500;
}

namespace app_list {

SearchController::SearchController(Profile* profile,
                                   SearchBoxModel* search_box,
                                   AppListModel::SearchResults* results,
                                   SpeechUIModel* speech_ui,
                                   AppListControllerDelegate* list_controller)
  : profile_(profile),
    search_box_(search_box),
    speech_ui_(speech_ui),
    list_controller_(list_controller),
    dispatching_query_(false),
    mixer_(new Mixer(results)),
    history_(HistoryFactory::GetForBrowserContext(profile)) {
  speech_ui_->AddObserver(this);
  Init();
}

SearchController::~SearchController() {
  speech_ui_->RemoveObserver(this);
}

void SearchController::Init() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  search_box_->SetIcon(*bundle.GetImageSkiaNamed(IDR_OMNIBOX_SEARCH));
  StartPageService* service = StartPageService::Get(profile_);
  if (service && service->GetSpeechRecognitionContents()) {
    search_box_->SetSpeechRecognitionButton(
        scoped_ptr<SearchBoxModel::SpeechButtonProperty>(
            new SearchBoxModel::SpeechButtonProperty(
                *bundle.GetImageSkiaNamed(IDR_OMNIBOX_MIC_SEARCH),
                l10n_util::GetStringUTF16(
                    IDS_APP_LIST_HOTWORD_LISTENING),
                *bundle.GetImageSkiaNamed(IDR_APP_LIST_MIC_HOTWORD_OFF),
                l10n_util::GetStringUTF16(
                    IDS_APP_LIST_START_SPEECH_RECOGNITION))));
  }
  OnSpeechRecognitionStateChanged(speech_ui_->state());

  mixer_->Init();

  AddProvider(Mixer::MAIN_GROUP, scoped_ptr<SearchProvider>(
      new AppSearchProvider(profile_, list_controller_)).Pass());
  AddProvider(Mixer::OMNIBOX_GROUP, scoped_ptr<SearchProvider>(
      new OmniboxProvider(profile_)).Pass());
  AddProvider(Mixer::WEBSTORE_GROUP, scoped_ptr<SearchProvider>(
      new WebstoreProvider(profile_, list_controller_)).Pass());
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisablePeopleSearch)) {
    AddProvider(Mixer::PEOPLE_GROUP, scoped_ptr<SearchProvider>(
        new PeopleProvider(profile_)).Pass());
  }
}

void SearchController::Start() {
  Stop();

  list_controller_->OnSearchStarted();

  base::string16 query;
  base::TrimWhitespace(search_box_->text(), base::TRIM_ALL, &query);

  dispatching_query_ = true;
  for (Providers::iterator it = providers_.begin();
       it != providers_.end();
       ++it) {
    (*it)->Start(query);
  }
  dispatching_query_ = false;

  OnResultsChanged();

  stop_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kStopTimeMS),
                    base::Bind(&SearchController::Stop,
                               base::Unretained(this)));
}

void SearchController::Stop() {
  stop_timer_.Stop();

  for (Providers::iterator it = providers_.begin();
       it != providers_.end();
       ++it) {
    (*it)->Stop();
  }
}

void SearchController::OpenResult(SearchResult* result, int event_flags) {
  // Count AppList.Search here because it is composed of search + action.
  content::RecordAction(base::UserMetricsAction("AppList_Search"));

  ChromeSearchResult* chrome_result =
      static_cast<app_list::ChromeSearchResult*>(result);
  UMA_HISTOGRAM_ENUMERATION(kAppListSearchResultOpenTypeHistogram,
                            chrome_result->GetType(),
                            SEARCH_RESULT_TYPE_BOUNDARY);
  chrome_result->Open(event_flags);

  if (history_ && history_->IsReady()) {
    history_->AddLaunchEvent(base::UTF16ToUTF8(search_box_->text()),
                             chrome_result->id());
  }
}

void SearchController::InvokeResultAction(SearchResult* result,
                                          int action_index,
                                          int event_flags) {
  // TODO(xiyuan): Hook up with user learning.
  static_cast<app_list::ChromeSearchResult*>(result)->InvokeAction(
      action_index, event_flags);
}

void SearchController::AddProvider(Mixer::GroupId group,
                                   scoped_ptr<SearchProvider> provider) {
  provider->set_result_changed_callback(base::Bind(
      &SearchController::OnResultsChanged,
      base::Unretained(this)));
  mixer_->AddProviderToGroup(group, provider.get());
  providers_.push_back(provider.release());  // Takes ownership.
}

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  KnownResults known_results;
  if (history_ && history_->IsReady()) {
    history_->GetKnownResults(base::UTF16ToUTF8(search_box_->text()))
        ->swap(known_results);
  }

  mixer_->MixAndPublish(known_results);
}

void SearchController::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  search_box_->SetHintText(l10n_util::GetStringUTF16(
      (new_state == SPEECH_RECOGNITION_HOTWORD_LISTENING) ?
      IDS_SEARCH_BOX_HOTWORD_HINT : IDS_SEARCH_BOX_HINT));
}

}  // namespace app_list
