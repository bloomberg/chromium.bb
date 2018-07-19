// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tasks/task_tab_helper.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"

namespace tasks {

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TaskTabHelper);

TaskTabHelper::TaskTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      last_pruned_navigation_entry_index_(-1) {}

TaskTabHelper::~TaskTabHelper() {}

TaskTabHelper::HubType TaskTabHelper::GetSpokeEntryHubType() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();

  DCHECK(entry);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  if (url_service && url_service->IsSearchResultsPageFromDefaultSearchProvider(
                         entry->GetURL())) {
    return HubType::DEFAULT_SEARCH_ENGINE;
  } else if (ui::PageTransitionCoreTypeIs(
                 entry->GetTransitionType(),
                 ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT)) {
    return HubType::FORM_SUBMIT;
  } else {
    return HubType::OTHER;
  }
}

void TaskTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();

  if (current_entry_index > last_pruned_navigation_entry_index_)
    entry_index_to_spoke_count_map_[current_entry_index] = 1;
}

void TaskTabHelper::NavigationListPruned(
    const content::PrunedDetails& pruned_details) {
  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();

  if (entry_index_to_spoke_count_map_.count(current_entry_index) == 0)
    entry_index_to_spoke_count_map_[current_entry_index] = 1;

  entry_index_to_spoke_count_map_[current_entry_index]++;
  last_pruned_navigation_entry_index_ = current_entry_index;

  RecordHubAndSpokeNavigationUsage(
      entry_index_to_spoke_count_map_[current_entry_index]);
}

void TaskTabHelper::RecordHubAndSpokeNavigationUsage(int spokes) {
  DCHECK_GT(spokes, 1);

  std::string histogram_name;
  switch (GetSpokeEntryHubType()) {
    case HubType::DEFAULT_SEARCH_ENGINE: {
      histogram_name =
          "Tabs.Tasks.HubAndSpokeNavigationUsage.FromDefaultSearchEngine";
      break;
    }
    case HubType::FORM_SUBMIT: {
      histogram_name = "Tabs.Tasks.HubAndSpokeNavigationUsage.FromFormSubmit";
      break;
    }
    case HubType::OTHER: {
      histogram_name = "Tabs.Tasks.HubAndSpokeNavigationUsage.FromOther";
      break;
    }
    default: { NOTREACHED() << "Unknown HubType"; }
  }

  base::UmaHistogramExactLinear(histogram_name, spokes, 100);
}

}  // namespace tasks
