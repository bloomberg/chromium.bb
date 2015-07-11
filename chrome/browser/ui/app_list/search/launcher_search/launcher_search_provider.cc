// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service.h"

using chromeos::launcher_search_provider::Service;

namespace app_list {

namespace {

const int kLauncherSearchProviderQueryDelayInMs = 100;
const int kLauncherSearchProviderMaxResults = 6;

}  // namespace

LauncherSearchProvider::LauncherSearchProvider(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
}

LauncherSearchProvider::~LauncherSearchProvider() {
}

void LauncherSearchProvider::Start(bool /*is_voice_query*/,
                                   const base::string16& query) {
  // Clear previously added search results.
  ClearResults();

  DelayQuery(base::Bind(&LauncherSearchProvider::StartInternal,
                        weak_ptr_factory_.GetWeakPtr(), query));
}

void LauncherSearchProvider::Stop() {
  // Since app_list code can call Stop() at any time, we stop timer here in
  // order not to start query after Stop() is called.
  query_timer_.Stop();

  // Clear all search results of the previous query. Since results are
  // duplicated when being exported from the map, there are no external pointers
  // to |extension_results_|, so it is safe to clear the map.
  extension_results_.clear();

  Service* service = Service::Get(profile_);

  // Since we delay queries and filter out empty string queries, it can happen
  // that no query is running at service side.
  if (service->IsQueryRunning())
    service->OnQueryEnded();
}

void LauncherSearchProvider::SetSearchResults(
    const extensions::ExtensionId& extension_id,
    ScopedVector<LauncherSearchResult> results) {
  DCHECK(Service::Get(profile_)->IsQueryRunning());

  // Add this extension's results (erasing any existing results).
  extension_results_.set(
      extension_id,
      make_scoped_ptr(new ScopedVector<LauncherSearchResult>(results.Pass())));

  // Update results with other extension results.
  ClearResults();
  for (const auto& item : extension_results_) {
    for (const auto* result : *item.second)
      Add(result->Duplicate());
  }
}

void LauncherSearchProvider::DelayQuery(const base::Closure& closure) {
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(kLauncherSearchProviderQueryDelayInMs);
  if (base::Time::Now() - last_query_time_ > delay) {
    query_timer_.Stop();
    closure.Run();
  } else {
    query_timer_.Start(FROM_HERE, delay, closure);
  }
  last_query_time_ = base::Time::Now();
}

void LauncherSearchProvider::StartInternal(const base::string16& query) {
  if (!query.empty()) {
    Service::Get(profile_)->OnQueryStarted(this, base::UTF16ToUTF8(query),
                                           kLauncherSearchProviderMaxResults);
  }
}

}  // namespace app_list
