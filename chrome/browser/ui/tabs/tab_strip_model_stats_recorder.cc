// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"

#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

TabStripModelStatsRecorder::TabStripModelStatsRecorder() {
  BrowserList::AddObserver(this);
}

TabStripModelStatsRecorder::~TabStripModelStatsRecorder() {
  for (chrome::BrowserIterator iterator; !iterator.done(); iterator.Next())
    iterator->tab_strip_model()->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
}

void TabStripModelStatsRecorder::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void TabStripModelStatsRecorder::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}
