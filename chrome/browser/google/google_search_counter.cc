// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_counter.h"

#include "base/logging.h"
#include "chrome/browser/google/google_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace {

// Returns true iff |entry| represents a Google search from the Omnibox.
// This method assumes that we have already verified that |entry|'s URL is a
// Google search URL.
bool IsOmniboxGoogleSearchNavigation(const content::NavigationEntry& entry) {
  const content::PageTransition stripped_transition =
      PageTransitionStripQualifier(entry.GetTransitionType());
  DCHECK(google_util::IsGoogleSearchUrl(entry.GetURL().spec()));
  return stripped_transition == content::PAGE_TRANSITION_GENERATED;
}

}  // namespace

// static
void GoogleSearchCounter::RegisterForNotifications() {
  GoogleSearchCounter::GetInstance()->RegisterForNotificationsInternal();
}

// static
GoogleSearchCounter* GoogleSearchCounter::GetInstance() {
  return Singleton<GoogleSearchCounter>::get();
}

GoogleSearchCounter::GoogleSearchCounter()
    : search_metrics_(new GoogleSearchMetrics) {
}

GoogleSearchCounter::~GoogleSearchCounter() {
}

void GoogleSearchCounter::ProcessCommittedEntry(
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const content::LoadCommittedDetails* commit =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  const content::NavigationEntry& entry = *commit->entry;

  // First see if this is a Google search URL at all.
  if (!google_util::IsGoogleSearchUrl(entry.GetURL().spec()))
    return;

  // If the commit is a GENERATED commit with a Google search URL, we know it's
  // an Omnibox search.
  if (IsOmniboxGoogleSearchNavigation(entry)) {
    // Note that GoogleSearchMetrics logs metrics through UMA, which will only
    // transmit these counts to the server if the user has opted into sending
    // usage stats.
    search_metrics_->RecordGoogleSearch(GoogleSearchMetrics::AP_OMNIBOX);
  } else {
    // For all other cases that we have not yet implemented or care to measure,
    // we log a generic "catch-all" metric.
    search_metrics_->RecordGoogleSearch(GoogleSearchMetrics::AP_OTHER);
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
