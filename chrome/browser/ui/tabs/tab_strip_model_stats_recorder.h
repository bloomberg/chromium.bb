// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_

#include "base/macros.h"

#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}

// TabStripModelStatsRecorder records user tab interaction stats.
// In particular, we record tab's lifetime and state transition probability to
// study user interaction with background tabs. (crbug.com/517335)
class TabStripModelStatsRecorder : public chrome::BrowserListObserver,
                                   public TabStripModelObserver {
 public:
  TabStripModelStatsRecorder();
  ~TabStripModelStatsRecorder() override;

 private:
  // chrome::BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelStatsRecorder);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_
