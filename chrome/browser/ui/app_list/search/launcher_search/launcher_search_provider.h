// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/app_list/search_provider.h"

namespace app_list {

// LauncherSearchProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class LauncherSearchProvider : public SearchProvider {
 public:
  explicit LauncherSearchProvider(Profile* profile);
  ~LauncherSearchProvider() override;

  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

 private:
  // Delays query for |kLauncherSearchProviderQueryDelayInMs|. This dispatches
  // the latest query after no more calls to Start() for the delay duration.
  void DelayQuery(const base::Closure& closure);

  // Dispatches |query| to LauncherSearchProvider service.
  void StartInternal(const base::string16& query);

  // A timer to delay query.
  base::OneShotTimer<LauncherSearchProvider> query_timer_;

  // The timestamp of the last query.
  base::Time last_query_time_;

  // Keep reference to profile to get LauncherSearchProvider service.
  Profile* profile_;

  base::WeakPtrFactory<LauncherSearchProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LauncherSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_PROVIDER_H_
