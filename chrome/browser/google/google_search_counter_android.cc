// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_counter_android.h"

#include "base/logging.h"
#include "chrome/browser/google/google_search_counter.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "components/google/core/browser/google_search_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

GoogleSearchCounterAndroid::GoogleSearchCounterAndroid(Profile* profile)
    : profile_(profile) {
  // We always listen for all COMMITTED navigations from all sources, as any
  // one of them could be a navigation of interest.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

GoogleSearchCounterAndroid::~GoogleSearchCounterAndroid() {
}

void GoogleSearchCounterAndroid::ProcessCommittedEntry(
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  GoogleSearchCounter* counter = GoogleSearchCounter::GetInstance();
  DCHECK(counter);
  if (!counter->ShouldRecordCommittedDetails(details))
    return;

  const content::NavigationEntry& entry =
      *content::Details<content::LoadCommittedDetails>(details)->entry;
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile_);
  DCHECK(prerender_manager);
  counter->search_metrics()->RecordAndroidGoogleSearch(
      counter->GetGoogleSearchAccessPointForSearchNavEntry(entry),
      prerender_manager->IsEnabled());
}

void GoogleSearchCounterAndroid::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  ProcessCommittedEntry(source, details);
}
