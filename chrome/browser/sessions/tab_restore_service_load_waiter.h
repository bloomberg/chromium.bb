// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_

#include "base/run_loop.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Browser;

// Class used to run a message loop waiting for the TabRestoreService to finish
// loading. Does nothing if the TabRestoreService was already loaded.
class TabRestoreServiceLoadWaiter : public sessions::TabRestoreServiceObserver {
 public:
  explicit TabRestoreServiceLoadWaiter(Browser* browser);

  ~TabRestoreServiceLoadWaiter() override;

  void Wait();

 private:
  // Overridden from TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {}
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  sessions::TabRestoreService* const tab_restore_service_;
  const bool do_wait_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreServiceLoadWaiter);
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_
