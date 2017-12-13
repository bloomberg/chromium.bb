// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace resource_coordinator {

TabLifecycleUnitSource::TabLifecycleUnitSource()
    : browser_tab_strip_tracker_(this, nullptr, this) {
  DCHECK(BrowserList::GetInstance()->empty());
  browser_tab_strip_tracker_.Init();
}

TabLifecycleUnitSource::~TabLifecycleUnitSource() = default;

void TabLifecycleUnitSource::AddTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  tab_lifecycle_observers_.AddObserver(observer);
}

void TabLifecycleUnitSource::RemoveTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  tab_lifecycle_observers_.RemoveObserver(observer);
}

void TabLifecycleUnitSource::SetFocusedTabStripModelForTesting(
    TabStripModel* tab_strip) {
  focused_tab_strip_model_for_testing_ = tab_strip;
  UpdateFocusedTab();
}

TabStripModel* TabLifecycleUnitSource::GetFocusedTabStripModel() const {
  if (focused_tab_strip_model_for_testing_)
    return focused_tab_strip_model_for_testing_;
  Browser* const focused_browser = chrome::FindBrowserWithActiveWindow();
  if (!focused_browser)
    return nullptr;
  return focused_browser->tab_strip_model();
}

void TabLifecycleUnitSource::UpdateFocusedTab() {
  TabStripModel* const focused_tab_strip_model = GetFocusedTabStripModel();
  content::WebContents* const focused_web_contents =
      focused_tab_strip_model ? focused_tab_strip_model->GetActiveWebContents()
                              : nullptr;
  DCHECK(!focused_web_contents ||
         base::ContainsKey(tabs_, focused_web_contents));
  UpdateFocusedTabTo(focused_web_contents ? tabs_[focused_web_contents].get()
                                          : nullptr);
}

void TabLifecycleUnitSource::UpdateFocusedTabTo(
    TabLifecycleUnit* new_focused_tab_lifecycle_unit) {
  if (new_focused_tab_lifecycle_unit == focused_tab_lifecycle_unit_)
    return;
  if (focused_tab_lifecycle_unit_)
    focused_tab_lifecycle_unit_->SetFocused(false);
  if (new_focused_tab_lifecycle_unit)
    new_focused_tab_lifecycle_unit->SetFocused(true);
  focused_tab_lifecycle_unit_ = new_focused_tab_lifecycle_unit;
}

void TabLifecycleUnitSource::TabInsertedAt(TabStripModel* tab_strip_model,
                                           content::WebContents* contents,
                                           int index,
                                           bool foreground) {
  auto it = tabs_.find(contents);
  if (it == tabs_.end()) {
    // A tab was created.
    auto res = tabs_.insert(std::make_pair(
        contents, std::make_unique<TabLifecycleUnit>(
                      &tab_lifecycle_observers_, contents, tab_strip_model)));
    if (GetFocusedTabStripModel() == tab_strip_model && foreground)
      UpdateFocusedTabTo(res.first->second.get());
    NotifyLifecycleUnitCreated(res.first->second.get());
  } else {
    // A tab was moved to another window.
    it->second->SetTabStripModel(tab_strip_model);
    if (foreground)
      UpdateFocusedTab();
  }
}

void TabLifecycleUnitSource::TabClosingAt(TabStripModel* tab_strip_model,
                                          content::WebContents* contents,
                                          int index) {
  auto it = tabs_.find(contents);
  DCHECK(it != tabs_.end());
  TabLifecycleUnit* lifecycle_unit = it->second.get();
  if (focused_tab_lifecycle_unit_ == lifecycle_unit)
    focused_tab_lifecycle_unit_ = nullptr;
  NotifyLifecycleUnitDestroyed(lifecycle_unit);
  tabs_.erase(contents);
}

void TabLifecycleUnitSource::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::TabReplacedAt(TabStripModel* tab_strip_model,
                                           content::WebContents* old_contents,
                                           content::WebContents* new_contents,
                                           int index) {
  DCHECK(!base::ContainsKey(tabs_, new_contents));
  auto it = tabs_.find(old_contents);
  DCHECK(it != tabs_.end());
  std::unique_ptr<TabLifecycleUnit> lifecycle_unit = std::move(it->second);
  lifecycle_unit->SetWebContents(new_contents);
  tabs_.erase(it);
  tabs_[new_contents] = std::move(lifecycle_unit);
}

void TabLifecycleUnitSource::OnBrowserSetLastActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnBrowserNoLongerActive(Browser* browser) {
  UpdateFocusedTab();
}

}  // namespace resource_coordinator
