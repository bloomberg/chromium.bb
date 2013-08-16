// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_

#include "base/memory/ref_counted.h"

class Browser;

// Manages confirming that browser windows are closeable and closing them at
// shutdown.
class BrowserCloseManager : public base::RefCounted<BrowserCloseManager> {
 public:
  BrowserCloseManager();

  // Starts closing all browser windows.
  void StartClosingBrowsers();

 private:
  friend class base::RefCounted<BrowserCloseManager>;

  virtual ~BrowserCloseManager();

  // Notifies all browser windows that the close is cancelled.
  void CancelBrowserClose();

  // Checks whether all browser windows are ready to close and closes them if
  // they are.
  void TryToCloseBrowsers();

  // Called to report whether a beforeunload dialog was accepted.
  void OnBrowserReportCloseable(bool proceed);

  // Closes all browser windows.
  void CloseBrowsers();

  // The browser for which we are waiting for a callback to
  // OnBrowserReportCloseable.
  Browser* current_browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCloseManager);
};

#endif  // CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_
