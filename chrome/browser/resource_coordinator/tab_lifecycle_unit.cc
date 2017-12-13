// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include "base/logging.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    base::ObserverList<TabLifecycleObserver>* observers,
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model)
    : observers_(observers),
      web_contents_(web_contents),
      tab_strip_model_(tab_strip_model) {
  DCHECK(observers_);
  DCHECK(GetWebContents());
  DCHECK(tab_strip_model_);
}

TabLifecycleUnitSource::TabLifecycleUnit::~TabLifecycleUnit() = default;

void TabLifecycleUnitSource::TabLifecycleUnit::SetTabStripModel(
    TabStripModel* tab_strip_model) {
  DCHECK(tab_strip_model);
  tab_strip_model = tab_strip_model_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetFocused(bool focused) {
  const bool was_focused = last_focused_time_ == base::TimeTicks::Max();
  if (focused == was_focused)
    return;
  last_focused_time_ = focused ? base::TimeTicks::Max() : NowTicks();

  // TODO(fdoray): Reload the tab if discarded. https://crbug.com/775644
}

base::string16 TabLifecycleUnitSource::TabLifecycleUnit::GetTitle() const {
  return GetWebContents()->GetTitle();
}

std::string TabLifecycleUnitSource::TabLifecycleUnit::GetIconURL() const {
  auto* last_committed_entry =
      GetWebContents()->GetController().GetLastCommittedEntry();
  if (!last_committed_entry)
    return std::string();
  const auto& favicon = last_committed_entry->GetFavicon();
  return favicon.valid ? favicon.url.spec() : std::string();
}

LifecycleUnit::SortKey TabLifecycleUnitSource::TabLifecycleUnit::GetSortKey()
    const {
  return SortKey(last_focused_time_);
}

LifecycleUnit::State TabLifecycleUnitSource::TabLifecycleUnit::GetState()
    const {
  return state_;
}

int TabLifecycleUnitSource::TabLifecycleUnit::
    GetEstimatedMemoryFreedOnDiscardKB() const {
  // TODO(fdoray): Implement this. https://crbug.com/775644
  return 0;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanDiscard(
    DiscardReason reason) const {
  // TODO(fdoray): Implement this. https://crbug.com/775644
  return false;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Discard(
    DiscardReason discard_reason) {
  // TODO(fdoray): Implement this. https://crbug.com/775644
  return false;
}

content::WebContents* TabLifecycleUnitSource::TabLifecycleUnit::GetWebContents()
    const {
  return web_contents_;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable)
    return;
  auto_discardable_ = auto_discardable;
  for (auto& observer : *observers_)
    observer.OnAutoDiscardableStateChange(GetWebContents(), auto_discardable_);
}

void TabLifecycleUnitSource::TabLifecycleUnit::DiscardTab() {
  Discard(DiscardReason::kExternal);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsDiscarded() const {
  return GetState() == State::DISCARDED;
}

}  // namespace resource_coordinator
