// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"

#include <utility>

#include "base/bind.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

ReopenTabInProductHelp::ReopenTabInProductHelp(Profile* profile,
                                               const base::TickClock* clock)
    : profile_(profile),
      clock_(clock),
      active_tab_tracker_(
          clock_,
          base::BindRepeating(&ReopenTabInProductHelp::OnActiveTabClosed,
                              base::Unretained(this))),
      trigger_(GetTracker(), clock_) {
  // We want to track all created and destroyed browsers, and also add all
  // currently living browsers.
  BrowserList::AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    active_tab_tracker_.AddTabStripModel(browser->tab_strip_model());
  }

  // |base::Unretained| is safe here since this is a member.
  trigger_.SetShowHelpCallback(base::BindRepeating(
      &ReopenTabInProductHelp::OnShowHelp, base::Unretained(this)));
}

ReopenTabInProductHelp::~ReopenTabInProductHelp() {
  BrowserList::RemoveObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    active_tab_tracker_.RemoveTabStripModel(browser->tab_strip_model());
  }
}

void ReopenTabInProductHelp::NewTabOpened() {
  trigger_.NewTabOpened();
}

void ReopenTabInProductHelp::TabReopened() {
  GetTracker()->NotifyEvent(feature_engagement::events::kTabReopened);
}

void ReopenTabInProductHelp::HelpDismissed() {
  trigger_.HelpDismissed();
}

void ReopenTabInProductHelp::OnActiveTabClosed(
    TabStripModel* tab_strip_model,
    base::TimeDelta active_duration) {
  trigger_.ActiveTabClosed(active_duration);
}

void ReopenTabInProductHelp::OnShowHelp() {
  auto* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  browser->window()->ShowInProductHelpPromo(InProductHelpFeature::kReopenTab);
}

void ReopenTabInProductHelp::OnBrowserAdded(Browser* browser) {
  active_tab_tracker_.AddTabStripModel(browser->tab_strip_model());
}

void ReopenTabInProductHelp::OnBrowserRemoved(Browser* browser) {
  active_tab_tracker_.RemoveTabStripModel(browser->tab_strip_model());
}

feature_engagement::Tracker* ReopenTabInProductHelp::GetTracker() {
  return feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
}
