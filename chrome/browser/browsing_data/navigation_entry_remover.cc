// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/navigation_entry_remover.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

bool TimeRangeMatcher(base::Time begin,
                      base::Time end,
                      const content::NavigationEntry& entry) {
  return begin <= entry.GetTimestamp() &&
         (entry.GetTimestamp() < end || end.is_null());
}

bool UrlMatcher(const base::flat_set<GURL>& urls,
                const content::NavigationEntry& entry) {
  return urls.find(entry.GetURL()) != urls.end();
}

// Create a predicate to select NavigationEntries that should be deleted.
// If a valid time_range is supplied, the decision will be based on time and
// |deleted_rows| is ignored.
// Otherwise entries matching |deleted_rows| will be selected.
content::NavigationController::DeletionPredicate CreateDeletionPredicate(
    const history::DeletionTimeRange& time_range,
    const history::URLRows& deleted_rows) {
  if (time_range.IsValid()) {
    return base::BindRepeating(&TimeRangeMatcher, time_range.begin(),
                               time_range.end());
  }
  std::vector<GURL> urls;
  for (const history::URLRow& row : deleted_rows) {
    urls.push_back(row.url());
  }
  return base::BindRepeating(&UrlMatcher,
                             base::flat_set<GURL>(std::move(urls)));
}

// Desktop is using |TabStripModel|, Android |TabModel|. They don't have a
// common base class but both have a |GetWebContentsAt()| method.
// TODO(dullweber): Add a common base class?
template <typename TabList>
void DeleteNavigationEntries(
    TabList* tab_list,
    int tab_count,
    const content::NavigationController::DeletionPredicate& predicate) {
  for (int i = 0; i < tab_count; i++) {
    content::WebContents* web_contents = tab_list->GetWebContentsAt(i);
    content::NavigationController* controller = &web_contents->GetController();
    controller->DiscardNonCommittedEntries();
    // We discarded pending and transient entries but there could still be
    // no last_committed_entry, which would prevent deletion.
    if (controller->CanPruneAllButLastCommitted())
      controller->DeleteNavigationEntries(predicate);
  }
}

}  // namespace

namespace browsing_data {

void RemoveNavigationEntries(Profile* profile,
                             const history::DeletionTimeRange& time_range,
                             const history::URLRows& deleted_rows) {
  DCHECK(profile->GetProfileType() == Profile::ProfileType::REGULAR_PROFILE);
  if (!base::FeatureList::IsEnabled(
          browsing_data::features::kRemoveNavigationHistory)) {
    return;
  }

  auto predicate = CreateDeletionPredicate(time_range, deleted_rows);

#if defined(OS_ANDROID)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    TabModel* tab_model = *it;
    if (tab_model->GetProfile() == profile) {
      DeleteNavigationEntries(tab_model, tab_model->GetTabCount(), predicate);
    }
  }
#else
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (browser->profile() == profile) {
      DeleteNavigationEntries(tab_strip, tab_strip->count(), predicate);
    }
  }
#endif
}

}  // namespace browsing_data
