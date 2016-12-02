// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry_loading_util.h"

#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"

namespace reading_list {

void LoadReadingListEntry(ReadingListEntry const& entry,
                          ReadingListModel* model,
                          web::WebState* web_state) {
  // TODO(crbug.com/625617): Evaluate whether NetworkChangeNotifier
  // correctly detects when users are offline.
  bool open_distilled_entry =
      net::NetworkChangeNotifier::IsOffline() &&
      entry.DistilledState() == ReadingListEntry::PROCESSED;
  if (open_distilled_entry) {
    return LoadReadingListDistilled(entry, model, web_state);
  }

  DCHECK(entry.URL().is_valid());
  web::NavigationManager::WebLoadParams params(entry.URL());
  params.transition_type = ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK;
  web_state->GetNavigationManager()->LoadURLWithParams(params);
  ReadingListWebStateObserver* web_state_observer =
      ReadingListWebStateObserver::FromWebState(web_state, model);
  web_state_observer->StartCheckingProgress();
  model->SetReadStatus(entry.URL(), true);
}

void LoadReadingListDistilled(ReadingListEntry const& entry,
                              ReadingListModel* model,
                              web::WebState* web_state) {
  DCHECK(entry.DistilledState() == ReadingListEntry::PROCESSED);
  GURL url =
      reading_list::DistilledURLForPath(entry.DistilledPath(), entry.URL());
  DCHECK(url.is_valid());
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK;
  web_state->GetNavigationManager()->LoadURLWithParams(params);
  model->SetReadStatus(entry.URL(), true);
}

}  //  namespace reading_list
