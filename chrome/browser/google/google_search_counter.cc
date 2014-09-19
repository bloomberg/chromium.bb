// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_counter.h"

#include "base/logging.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

// static
void GoogleSearchCounter::RegisterForNotifications() {
  GoogleSearchCounter::GetInstance()->RegisterForNotificationsInternal();
}

// static
GoogleSearchCounter* GoogleSearchCounter::GetInstance() {
  return Singleton<GoogleSearchCounter>::get();
}

GoogleSearchMetrics::AccessPoint
GoogleSearchCounter::GetGoogleSearchAccessPointForSearchNavEntry(
    const content::NavigationEntry& entry) const {
  DCHECK(google_util::IsGoogleSearchUrl(entry.GetURL()));

  // If the |entry| is FROM_ADDRESS_BAR, it comes from the omnibox; if it's
  // GENERATED, the user was doing a search, rather than doing a navigation to a
  // search URL (e.g. from hisotry, or pasted in).
  if (entry.GetTransitionType() == (ui::PAGE_TRANSITION_GENERATED |
      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    return GoogleSearchMetrics::AP_OMNIBOX;
  }

  // The string "source=search_app" in the |entry| URL represents a Google
  // search from the Google Search App.
  if (entry.GetURL().query().find("source=search_app") != std::string::npos)
    return GoogleSearchMetrics::AP_SEARCH_APP;

  // For all other cases that we have not yet implemented or care to measure, we
  // log a generic "catch-all" metric.
  return GoogleSearchMetrics::AP_OTHER;
}

bool GoogleSearchCounter::ShouldRecordCommittedDetails(
    const content::NotificationDetails& details) const {
  const content::LoadCommittedDetails* commit =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  return google_util::IsGoogleSearchUrl(commit->entry->GetURL());
}

GoogleSearchCounter::GoogleSearchCounter()
    : search_metrics_(new GoogleSearchMetrics) {
}

GoogleSearchCounter::~GoogleSearchCounter() {
}

void GoogleSearchCounter::ProcessCommittedEntry(
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Note that GoogleSearchMetrics logs metrics through UMA, which will only
  // transmit these counts to the server if the user has opted into sending
  // usage stats.
  const content::LoadCommittedDetails* commit =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  const content::NavigationEntry& entry = *commit->entry;
  if (ShouldRecordCommittedDetails(details)) {
    search_metrics_->RecordGoogleSearch(
        GetGoogleSearchAccessPointForSearchNavEntry(entry));
  }
}

void GoogleSearchCounter::SetSearchMetricsForTesting(
    GoogleSearchMetrics* search_metrics) {
  DCHECK(search_metrics);
  search_metrics_.reset(search_metrics);
}

void GoogleSearchCounter::RegisterForNotificationsInternal() {
  // We always listen for all COMMITTED navigations from all sources, as any
  // one of them could be a navigation of interest.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

void GoogleSearchCounter::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
      ProcessCommittedEntry(source, details);
      break;
    default:
      NOTREACHED();
  }
}
