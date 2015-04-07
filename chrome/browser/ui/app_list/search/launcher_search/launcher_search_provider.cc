// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/service.h"

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
  DelayQuery(base::Bind(&LauncherSearchProvider::StartInternal,
                        weak_ptr_factory_.GetWeakPtr(), query));
}

void LauncherSearchProvider::Stop() {
  // Since app_list code can call Stop() at any time, we stop timer here in
  // order not to start query after Stop() is called.
  query_timer_.Stop();

  Service* service = Service::Get(profile_);

  // Since we delay queries and filter out empty string queries, it can happen
  // that no query is running at service side.
  if (service->IsQueryRunning())
    service->OnQueryEnded();
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
    Service::Get(profile_)->OnQueryStarted(base::UTF16ToUTF8(query),
                                           kLauncherSearchProviderMaxResults);
  }
}

}  // namespace app_list
