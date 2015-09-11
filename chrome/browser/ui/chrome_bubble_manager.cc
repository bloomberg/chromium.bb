// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_bubble_manager.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

// Add any new enum before BUBBLE_TYPE_MAX and update BubbleType in
// tools/metrics/histograms/histograms.xml.
// Do not re-order these because they are used for collecting metrics.
enum BubbleType {
  BUBBLE_TYPE_UNKNOWN,  // Used for unrecognized bubble names.
  BUBBLE_TYPE_MOCK,     // Used for testing.
  BUBBLE_TYPE_MAX,      // Number of bubble types; used for metrics.
};

// Convert from bubble name to ID. The bubble ID will allow collecting the
// close reason for each bubble type.
static int GetBubbleId(BubbleReference bubble) {
  BubbleType bubble_type = BUBBLE_TYPE_UNKNOWN;

  // Translate from bubble name to enum.
  if (bubble->GetName().compare("MockBubble"))
    bubble_type = BUBBLE_TYPE_MOCK;

  DCHECK_NE(bubble_type, BUBBLE_TYPE_UNKNOWN);
  DCHECK_NE(bubble_type, BUBBLE_TYPE_MAX);

  return bubble_type;
}

// Get the UMA title for this close reason.
static std::string GetBubbleCloseReasonName(BubbleCloseReason reason) {
  switch (reason) {
    case BUBBLE_CLOSE_FORCED: return "Bubbles.Close.Forced";
    case BUBBLE_CLOSE_FOCUS_LOST: return "Bubbles.Close.FocusLost";
    case BUBBLE_CLOSE_TABSWITCHED: return "Bubbles.Close.TabSwitched";
    case BUBBLE_CLOSE_TABDETACHED: return "Bubbles.Close.TabDetached";
    case BUBBLE_CLOSE_USER_DISMISSED: return "Bubbles.Close.UserDismissed";
    case BUBBLE_CLOSE_NAVIGATED: return "Bubbles.Close.Navigated";
    case BUBBLE_CLOSE_FULLSCREEN_TOGGLED:
      return "Bubbles.Close.FullscreenToggled";
    case BUBBLE_CLOSE_ACCEPTED: return "Bubbles.Close.Accepted";
    case BUBBLE_CLOSE_CANCELED: return "Bubbles.Close.Canceled";
  }

  NOTREACHED();
  return "Bubbles.Close.Unknown";
}

}  // namespace

ChromeBubbleManager::ChromeBubbleManager(TabStripModel* tab_strip_model)
    : tab_strip_model_(tab_strip_model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(tab_strip_model_);
  tab_strip_model_->AddObserver(this);
  AddBubbleManagerObserver(&chrome_bubble_metrics_);
}

ChromeBubbleManager::~ChromeBubbleManager() {
  tab_strip_model_->RemoveObserver(this);

  // Finalize requests before removing the BubbleManagerObserver so it can
  // collect metrics when closing any open bubbles.
  FinalizePendingRequests();
  RemoveBubbleManagerObserver(&chrome_bubble_metrics_);
}

void ChromeBubbleManager::TabDetachedAt(content::WebContents* contents,
                                        int index) {
  CloseAllBubbles(BUBBLE_CLOSE_TABDETACHED);
  // Any bubble that didn't close should update its anchor position.
  UpdateAllBubbleAnchors();
}

void ChromeBubbleManager::TabDeactivated(content::WebContents* contents) {
  CloseAllBubbles(BUBBLE_CLOSE_TABSWITCHED);
}

void ChromeBubbleManager::ActiveTabChanged(content::WebContents* old_contents,
                                           content::WebContents* new_contents,
                                           int index,
                                           int reason) {
  Observe(new_contents);
}

void ChromeBubbleManager::DidToggleFullscreenModeForTab(
    bool entered_fullscreen) {
  CloseAllBubbles(BUBBLE_CLOSE_FULLSCREEN_TOGGLED);
  // Any bubble that didn't close should update its anchor position.
  UpdateAllBubbleAnchors();
}

void ChromeBubbleManager::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  CloseAllBubbles(BUBBLE_CLOSE_NAVIGATED);
}

void ChromeBubbleManager::ChromeBubbleMetrics::OnBubbleNeverShown(
    BubbleReference bubble) {
  UMA_HISTOGRAM_ENUMERATION("Bubbles.NeverShown", GetBubbleId(bubble),
                            BUBBLE_TYPE_MAX);
}

void ChromeBubbleManager::ChromeBubbleMetrics::OnBubbleClosed(
    BubbleReference bubble, BubbleCloseReason reason) {
  // Log the amount of time the bubble was visible.
  base::TimeDelta visible_time = bubble->GetVisibleTime();
  UMA_HISTOGRAM_LONG_TIMES("Bubbles.DisplayTime.All", visible_time);

  // Log the reason the bubble was closed.
  UMA_HISTOGRAM_ENUMERATION(GetBubbleCloseReasonName(reason),
                            GetBubbleId(bubble),
                            BUBBLE_TYPE_MAX);
}
