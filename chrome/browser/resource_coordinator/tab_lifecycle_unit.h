// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabLifecycleObserver;

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnit,
      public TabLifecycleUnitExternal {
 public:
  // |observers| is a list of observers to notify when the discarded state or
  // the auto-discardable state of this tab changes. It can be modified outside
  // of this TabLifecycleUnit, but only on the sequence on which this
  // constructor is invoked. |web_contents| and |tab_strip_model| are the
  // WebContents and TabStripModel associated with this tab.
  TabLifecycleUnit(base::ObserverList<TabLifecycleObserver>* observers,
                   content::WebContents* web_contents,
                   TabStripModel* tab_strip_model);
  ~TabLifecycleUnit() override;

  // Sets the TabStripModel associated with this tab. The source that created
  // this TabLifecycleUnit is responsible for calling this when the tab moves to
  // a different TabStripModel.
  void SetTabStripModel(TabStripModel* tab_strip_model);

  // Sets the WebContents associated with this tab. The source that created this
  // TabLifecycleUnit is responsible for calling this when the tab's WebContents
  // changes (e.g. when the tab is discarded or when prerendered or distilled
  // content is displayed).
  void SetWebContents(content::WebContents* web_contents);

  // Invoked when the tab gains or loses focus.
  void SetFocused(bool focused);

  // LifecycleUnit:
  base::string16 GetTitle() const override;
  std::string GetIconURL() const override;
  SortKey GetSortKey() const override;
  State GetState() const override;
  int GetEstimatedMemoryFreedOnDiscardKB() const override;
  bool CanDiscard(DiscardReason reason) const override;
  bool Discard(DiscardReason discard_reason) override;

  // TabLifecycleUnitExternal:
  content::WebContents* GetWebContents() const override;
  bool IsAutoDiscardable() const override;
  void SetAutoDiscardable(bool auto_discardable) override;
  void DiscardTab() override;
  bool IsDiscarded() const override;

 private:
  // List of observers to notify when the discarded state or the auto-
  // discardable state of this tab changes.
  base::ObserverList<TabLifecycleObserver>* observers_;

  // The WebContents associated with this tab.
  content::WebContents* web_contents_;

  // TabStripModel to which this tab belongs.
  TabStripModel* tab_strip_model_;

  // Current state of this tab.
  State state_ = State::LOADED;

  // Last time at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused.
  base::TimeTicks last_focused_time_;

  // When this is false, CanDiscard() always returns false.
  bool auto_discardable_ = true;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
