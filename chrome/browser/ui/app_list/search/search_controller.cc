// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
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

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  mixer_->MixAndPublish();
}

}  // namespace app_list
