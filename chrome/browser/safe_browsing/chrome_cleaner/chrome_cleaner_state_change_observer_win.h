// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_STATE_CHANGE_OBSERVER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_STATE_CHANGE_OBSERVER_WIN_H_

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results.h"

namespace safe_browsing {

// A helper class which automatically starts observing |ChromeCleanerController|
// when instantiated and calls the given callback with new |show_cleanup| and
// |powered_by_partner| values if either of them changed.
// Un-registerers itself as an observer on deletion.
// Since ChromeCleanupController lives and has its methods called on the UI
// thread, this class should live on and be destroyed on the UI thread as well
// to guarantee that a state change does not race the destructor.
class ChromeCleanerStateChangeObserver
    : public ChromeCleanerController::Observer {
 public:
  typedef base::RepeatingCallback<void(bool showCleanup, bool partnerPowered)>
      OnShowCleanupUIChangeCallback;

  ChromeCleanerStateChangeObserver(
      const OnShowCleanupUIChangeCallback& on_state_change);
  ~ChromeCleanerStateChangeObserver() override;

  // ChromeCleanerController::Observer implementation.
  void OnIdle(ChromeCleanerController::IdleReason idle_reason) override;
  void OnScanning() override;
  void OnReporterRunning() override;
  void OnInfected(const ChromeCleanerScannerResults& scanner_results) override;
  void OnCleaning(const ChromeCleanerScannerResults& scanner_results) override;
  void OnRebootRequired() override;
  void OnLogsEnabledChanged(bool logs_enabled) override;

 private:
  OnShowCleanupUIChangeCallback on_show_cleanup_ui_change_;

  // Will be called when any of the Observer override methods are called.
  void OnCleanupStateChange();

  // Raw pointer to a singleton. Must outlive this object.
  ChromeCleanerController* controller_;
  bool cached_should_show_cleanup_in_settings_ui_;
  bool cached_cleanup_powered_by_partner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerStateChangeObserver);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_STATE_CHANGE_OBSERVER_WIN_H_
