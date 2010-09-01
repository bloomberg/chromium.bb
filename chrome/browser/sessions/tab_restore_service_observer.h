// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_OBSERVER_H_

class TabRestoreService;

// Observer is notified when the set of entries managed by TabRestoreService
// changes in some way.
class TabRestoreServiceObserver {
 public:
  // Sent when the set of entries changes in some way.
  virtual void TabRestoreServiceChanged(TabRestoreService* service) = 0;

  // Sent to all remaining Observers when TabRestoreService's
  // destructor is run.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) = 0;

 protected:
  virtual ~TabRestoreServiceObserver() {}
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_OBSERVER_H_
