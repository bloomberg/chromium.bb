// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_bubble_manager.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

ChromeBubbleManager::ChromeBubbleManager(TabStripModel* tab_strip_model)
    : tab_strip_model_(tab_strip_model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(tab_strip_model_);
  tab_strip_model_->AddObserver(this);
}

ChromeBubbleManager::~ChromeBubbleManager() {
  tab_strip_model_->RemoveObserver(this);
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
