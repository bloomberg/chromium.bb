// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

// Manages confirming that browser windows are closeable and closing them at
// shutdown.
class BrowserCloseManager : public base::RefCounted<BrowserCloseManager>,
                            public chrome::BrowserListObserver {
 public:
  BrowserCloseManager();

  // Starts closing all browser windows.
  void StartClosingBrowsers();

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 protected:
  friend class base::RefCounted<BrowserCloseManager>;

  ~BrowserCloseManager() override;

  virtual void ConfirmCloseWithPendingDownloads(
      int download_count,
      const base::Callback<void(bool)>& callback);

 private:
  // Notifies all browser windows that the close is cancelled.
  void CancelBrowserClose();

  // Checks whether all browser windows are ready to close and closes them if
  // they are.
  void TryToCloseBrowsers();

  // Called to report whether a beforeunload dialog was accepted.
  void OnBrowserReportCloseable(bool proceed);

  // Closes all browser windows.
  void CloseBrowsers();

  // Checks whether there are any downloads in-progress and prompts the user to
  // cancel them. If there are no downloads or the user accepts the cancel
  // downloads dialog, CloseBrowsers is called to continue with the shutdown.
  // Otherwise, if the user declines to cancel downloads, the shutdown is
  // aborted and the downloads page is shown for each profile with in-progress
  // downloads.
  void CheckForDownloadsInProgress();

  // Called to report whether downloads may be cancelled during shutdown.
  void OnReportDownloadsCancellable(bool proceed);

  // The browser for which we are waiting for a callback to
  // OnBrowserReportCloseable.
  Browser* current_browser_;

  // Whether we are currently iterating over browsers in CloseBrowsers().
  // No Browsers should be added or removed synchronously or concurrently,
  // except explicitly by CloseBrowsers().
  // This was added to investigate http://crbug.com/484951.
  // TODO(jackhou): Delete when no longer needed.
  bool iterating_over_browsers_during_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCloseManager);
};

#endif  // CHROME_BROWSER_LIFETIME_BROWSER_CLOSE_MANAGER_H_
