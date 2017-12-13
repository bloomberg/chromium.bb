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

// Time during which a tab cannot be discarded after having been focused.
static constexpr base::TimeDelta kTabFocusedProtectionTime =
    base::TimeDelta::FromMinutes(10);

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnit,
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

  // Sets the "recently audible" state of this tab. A tab is "recently audible"
  // if a speaker icon is displayed next to it in the tab strip. The source that
  // created this TabLifecycleUnit is responsible for calling this when the
  // "recently audible" state of the tab changes.
  void SetRecentlyAudible(bool recently_audible);

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
  // Invoked when the state goes from DISCARDED to non-DISCARDED and vice-versa.
  void OnDiscardedStateChange();

  // Whether the tab is playing audio, has played audio recently, is accessing
  // the microphone, is accessing the camera or is being mirrored.
  bool IsMediaTab() const;

  // Returns the RenderProcessHost associated with this tab.
  content::RenderProcessHost* GetRenderProcessHost() const;

  // content::WebContentsObserver:
  void DidStartLoading() override;

  // List of observers to notify when the discarded state or the auto-
  // discardable state of this tab changes.
  base::ObserverList<TabLifecycleObserver>* observers_;

  // TabStripModel to which this tab belongs.
  TabStripModel* tab_strip_model_;

  // Current state of this tab.
  State state_ = State::LOADED;

  // The number of times that this tab has been discarded.
  int discard_count_ = 0;

  // Last time at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused.
  base::TimeTicks last_focused_time_;

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
