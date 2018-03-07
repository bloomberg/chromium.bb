// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents_observer.h"

namespace resource_coordinator {

namespace {
TabLifecycleUnitSource* instance_ = nullptr;
}  // namespace

TabLifecycleUnitSource::TabLifecycleUnitSource()
    : browser_tab_strip_tracker_(this, nullptr, this) {
  DCHECK(!instance_);
  DCHECK(BrowserList::GetInstance()->empty());
  browser_tab_strip_tracker_.Init();
  instance_ = this;
}

TabLifecycleUnitSource::~TabLifecycleUnitSource() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
TabLifecycleUnitSource* TabLifecycleUnitSource::GetInstance() {
  return instance_;
}

TabLifecycleUnitExternal* TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
    content::WebContents* web_contents) const {
  auto it = tabs_.find(web_contents);
  if (it == tabs_.end())
    return nullptr;
  return it->second.get();
}

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

// A specialization of content::WebContentsObserver that allows to properly
// track the destruction of tabs.
class TabLifecycleUnitSource::TabLifecycleUnitWebContentsObserver
    : public content::WebContentsObserver {
 public:
  TabLifecycleUnitWebContentsObserver(
      content::WebContents* web_contents,
      TabLifecycleUnit* lifecycle_unit,
      TabLifecycleUnitSource* lifecycle_unit_source)
      : content::WebContentsObserver(web_contents),
        lifecycle_unit_(lifecycle_unit),
        lifecycle_unit_source_(lifecycle_unit_source) {}

  void DidStartLoading() override {
    DCHECK(lifecycle_unit_);
    lifecycle_unit_->OnDidStartLoading();
  }

  void WebContentsDestroyed() override {
    lifecycle_unit_source_->OnWebContentsDestroyed(web_contents());
    // The call above will free |this| and so nothing should be done on this
    // object starting from here.
  }

 private:
  // The lifecycle unit for this tab.
  TabLifecycleUnit* lifecycle_unit_;

  // The lifecycle unit source that should get notified when the observed
  // WebContents is about to be destroyed.
  TabLifecycleUnitSource* lifecycle_unit_source_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitWebContentsObserver);
};

TabStripModel* TabLifecycleUnitSource::GetFocusedTabStripModel() const {
  if (focused_tab_strip_model_for_testing_)
    return focused_tab_strip_model_for_testing_;
  // Use FindLastActive() rather than FindBrowserWithActiveWindow() to avoid
  // flakiness when focus changes during browser tests.
  Browser* const focused_browser = chrome::FindLastActive();
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
    TabLifecycleUnit* lifecycle_unit = res.first->second.get();
    if (GetFocusedTabStripModel() == tab_strip_model && foreground)
      UpdateFocusedTabTo(lifecycle_unit);

    // Add a self-owned observer to the LifecycleUnit to record metrics.
    lifecycle_unit->AddObserver(new DiscardMetricsLifecycleUnitObserver());

    // Track the WebContents used by this tab.
    web_contents_observers_.insert(std::make_pair(
        contents, std::make_unique<TabLifecycleUnitWebContentsObserver>(
                      contents, lifecycle_unit, this)));

    NotifyLifecycleUnitCreated(lifecycle_unit);
  } else {
    // A tab was moved to another window.
    it->second->SetTabStripModel(tab_strip_model);
    if (foreground)
      UpdateFocusedTab();
  }
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
  web_contents_observers_.erase(old_contents);
  tabs_.erase(it);
  web_contents_observers_.insert(std::make_pair(
      new_contents, std::make_unique<TabLifecycleUnitWebContentsObserver>(
                        new_contents, lifecycle_unit.get(), this)));
  tabs_[new_contents] = std::move(lifecycle_unit);
}

void TabLifecycleUnitSource::TabChangedAt(content::WebContents* contents,
                                          int index,
                                          TabChangeType change_type) {
  if (change_type != TabChangeType::kAll)
    return;
  auto it = tabs_.find(contents);
  // The WebContents destructor might cause this function to be called, at this
  // point TabClosingAt has already been called and so this WebContents has
  // been removed from |tabs_|.
  if (it == tabs_.end())
    return;
  TabLifecycleUnit* lifecycle_unit = it->second.get();
  lifecycle_unit->SetRecentlyAudible(contents->WasRecentlyAudible());
}

void TabLifecycleUnitSource::OnBrowserSetLastActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnBrowserNoLongerActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnWebContentsDestroyed(
    content::WebContents* contents) {
  auto it = tabs_.find(contents);
  DCHECK(it != tabs_.end());
  TabLifecycleUnit* lifecycle_unit = it->second.get();
  if (focused_tab_lifecycle_unit_ == lifecycle_unit)
    focused_tab_lifecycle_unit_ = nullptr;
  NotifyLifecycleUnitDestroyed(lifecycle_unit);
  tabs_.erase(contents);
}

}  // namespace resource_coordinator
