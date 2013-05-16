// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "ui/app_list/search_box_model.h"

namespace app_list {

SearchController::SearchController(Profile* profile,
                                   SearchBoxModel* search_box,
                                   AppListModel::SearchResults* results,
                                   AppListControllerDelegate* list_controller)
  : profile_(profile),
    search_box_(search_box),
    list_controller_(list_controller),
    dispatching_query_(false),
    mixer_(new Mixer(results)) {
  Init();
}

SearchController::~SearchController() {}

void SearchController::Init() {
  mixer_->Init();

  AddProvider(Mixer::MAIN_GROUP,
              scoped_ptr<SearchProvider>(
                  new AppSearchProvider(profile_, list_controller_)).Pass());
  AddProvider(Mixer::OMNIBOX_GROUP,
              scoped_ptr<SearchProvider>(new OmniboxProvider(profile_)).Pass());

  // TODO(xiyuan): Add providers.
}

void SearchController::Start() {
  Stop();

  string16 query;
  TrimWhitespace(search_box_->text(), TRIM_ALL, &query);

  dispatching_query_ = true;
  for (Providers::iterator it = providers_.begin();
       it != providers_.end();
       ++it) {
    (*it)->Start(query);
  }
  dispatching_query_ = false;

  OnResultsChanged();

  // Maximum time (in milliseconds) to wait to the search providers to finish.
  const int kStopTimeMS = 1500;
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
  // TODO(xiyuan): Hook up with user learning.
  static_cast<app_list::ChromeSearchResult*>(result)->Open(event_flags);
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

  mixer_->MixAndPublish();
}

}  // namespace app_list
