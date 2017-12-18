// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_

#include <stddef.h>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/sessions/core/session_id.h"

class BrowserList;

// Observes browser window activity in order to log WindowMetrics UKMs for
// browser events relative to tab activation and discarding.
// Multiple tabs in the same browser can refer to the same WindowMetrics entry.
// Must be used on the UI thread.
// TODO(michaelpg): Observe app and ARC++ windows as well.
class WindowActivityWatcher : public BrowserListObserver {
 public:
  // Represents a UKM entry for window metrics.
  struct WindowMetrics;

  // Returns the single instance, creating it if necessary.
  static WindowActivityWatcher* GetInstance();

  // Ensures the window's current stats are logged.
  // A new UKM entry will only be logged if an existing entry for the same
  // window doesn't exist yet or has stale properties.
  void CreateOrUpdateWindowMetrics(const Browser* browser);

 private:
  WindowActivityWatcher();
  ~WindowActivityWatcher() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  ScopedObserver<BrowserList, BrowserListObserver> browser_list_observer_;

  // Cache of WindowMetrics logged for each window. Used to avoid logging
  // duplicate identical UKM events.
  base::flat_map<SessionID::id_type, WindowMetrics> window_metrics_;

  DISALLOW_COPY_AND_ASSIGN(WindowActivityWatcher);
};

#endif  // CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_
