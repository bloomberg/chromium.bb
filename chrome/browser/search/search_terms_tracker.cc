// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_terms_tracker.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

SearchTermsTracker::SearchTermsTracker() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllSources());
}

SearchTermsTracker::~SearchTermsTracker() {
}

bool SearchTermsTracker::GetSearchTerms(
    const content::WebContents* contents,
    base::string16* search_terms,
    int* navigation_index) const {
  if (contents) {
    TabState::const_iterator it = tabs_.find(contents);
    if (it != tabs_.end() && !it->second.search_terms.empty()) {
      if (search_terms)
        *search_terms = it->second.search_terms;
      if (navigation_index)
        *navigation_index = it->second.srp_navigation_index;
      return true;
    }
  }
  return false;
}

void SearchTermsTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
    case content::NOTIFICATION_NAV_ENTRY_CHANGED: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      TabData search;
      if (FindMostRecentSearch(controller, &search))
        tabs_[controller->GetWebContents()] = search;
      else
        RemoveTabData(controller->GetWebContents());
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      RemoveTabData(content::Source<content::WebContents>(source).ptr());
      break;
    }

    default:
      NOTREACHED();
      return;
  }
}

bool SearchTermsTracker::FindMostRecentSearch(
    const content::NavigationController* controller,
    SearchTermsTracker::TabData* tab_data) {
  Profile* profile =
      Profile::FromBrowserContext(controller->GetBrowserContext());
  DCHECK(profile);
  if (!profile)
    return false;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  TemplateURL* template_url = service->GetDefaultSearchProvider();

  for (int i = controller->GetCurrentEntryIndex(); i >= 0; --i) {
    content::NavigationEntry* entry = controller->GetEntryAtIndex(i);
    if (entry->GetPageType() == content::PAGE_TYPE_NORMAL) {
      if (template_url->IsSearchURL(entry->GetURL(),
                                    service->search_terms_data())) {
        // This entry is a search results page. Extract the terms only if this
        // isn't the last (i.e. most recent/current) entry as we don't want to
        // record the search terms when we're on an SRP.
        if (i != controller->GetCurrentEntryIndex()) {
          tab_data->srp_navigation_index = i;
          template_url->ExtractSearchTermsFromURL(entry->GetURL(),
                                                  service->search_terms_data(),
                                                  &tab_data->search_terms);
          return true;
        }

        // We've found an SRP - stop searching (even if we did not record the
        // search terms, as anything before this entry will be unrelated).
        break;
      }
    }

    // Do not consider any navigations that precede a non-web-triggerable
    // navigation as they are not related to those terms.
    if (!content::PageTransitionIsWebTriggerable(
            entry->GetTransitionType())) {
      break;
    }
  }

  return false;
}

void SearchTermsTracker::RemoveTabData(
    const content::WebContents* contents) {
  TabState::iterator it = tabs_.find(contents);
  if (it != tabs_.end()) {
    tabs_.erase(it);
  }
}

}  // namespace chrome
