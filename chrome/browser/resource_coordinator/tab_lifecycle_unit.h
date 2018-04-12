// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

class TabStripModel;

namespace content {
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabLifecycleObserver;

// Time during which a tab cannot be discarded after having played audio.
static constexpr base::TimeDelta kTabAudioProtectionTime =
    base::TimeDelta::FromMinutes(1);

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnitBase,
      public TabLifecycleUnitExternal,
      public content::WebContentsObserver {
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
  // this TabLifecycleUnit is responsible for calling this when the tab is
  // removed from a TabStripModel or inserted into a new TabStripModel.
  void SetTabStripModel(TabStripModel* tab_strip_model);

  // Sets the WebContents associated with this tab. The source that created this
  // TabLifecycleUnit is responsible for calling this when the tab's WebContents
  // changes (e.g. when the tab is discarded or when prerendered or distilled
  // content is displayed).
  void SetWebContents(content::WebContents* web_contents);

  // Invoked when the tab gains or loses focus.
  void SetFocused(bool focused);

  // Sets the "recently audible" state of this tab. A tab is "recently audible"
  // if a speaker icon is displayed next to it in the tab strip. The source that
  // created this TabLifecycleUnit is responsible for calling this when the
  // "recently audible" state of the tab changes.
  void SetRecentlyAudible(bool recently_audible);

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::string16 GetTitle() const override;
  std::string GetIconURL() const override;
  base::ProcessHandle GetProcessHandle() const override;
  SortKey GetSortKey() const override;
  content::Visibility GetVisibility() const override;
  bool Freeze() override;
  int GetEstimatedMemoryFreedOnDiscardKB() const override;
  bool CanPurge() const override;
  bool CanDiscard(DiscardReason reason) const override;
  bool Discard(DiscardReason discard_reason) override;

  // TabLifecycleUnitExternal:
  content::WebContents* GetWebContents() const override;
  bool IsMediaTab() const override;
  bool IsAutoDiscardable() const override;
  void SetAutoDiscardable(bool auto_discardable) override;
  bool FreezeTab() override;
  bool DiscardTab() override;
  bool IsDiscarded() const override;
  int GetDiscardCount() const override;

 private:
  // Invoked when the state goes from DISCARDED to non-DISCARDED and vice-versa.
  void OnDiscardedStateChange();

  // Returns the RenderProcessHost associated with this tab.
  content::RenderProcessHost* GetRenderProcessHost() const;

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // List of observers to notify when the discarded state or the auto-
  // discardable state of this tab changes.
  base::ObserverList<TabLifecycleObserver>* observers_;

  // TabStripModel to which this tab belongs.
  TabStripModel* tab_strip_model_;

  // The number of times that this tab has been discarded.
  int discard_count_ = 0;

  // Last time at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused.
  //
  // TODO(fdoray): To keep old behavior (sort order and protection of recently
  // focused tabs), this is initialized with NowTicks(). Consider initializing
  // this with a null TimeTicks when the tab isn't initially focused.
  // https://crbug.com/800885
  base::TimeTicks last_focused_time_ = NowTicks();

  // When this is false, CanDiscard() always returns false.
  bool auto_discardable_ = true;

  // TimeTicks::Max() if the tab is currently "recently audible", null
  // TimeTicks() if the tab was never "recently audible", last time at which the
  // tab was "recently audible" otherwise.
  base::TimeTicks recently_audible_time_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
