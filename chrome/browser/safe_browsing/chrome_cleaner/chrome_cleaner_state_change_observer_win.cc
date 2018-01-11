// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_state_change_observer_win.h"

#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

ChromeCleanerStateChangeObserver::ChromeCleanerStateChangeObserver(
    const OnShowCleanupUIChangeCallback& on_show_cleanup_ui_change)
    : on_show_cleanup_ui_change_(on_show_cleanup_ui_change),
      controller_(ChromeCleanerController::GetInstance()),
      cached_should_show_cleanup_in_settings_ui_(
          controller_->ShouldShowCleanupInSettingsUI()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  controller_->AddObserver(this);
}

ChromeCleanerStateChangeObserver::~ChromeCleanerStateChangeObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  controller_->RemoveObserver(this);
}

void ChromeCleanerStateChangeObserver::OnCleanupStateChange() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool show_cleanup = controller_->ShouldShowCleanupInSettingsUI();

  // Avoid calling the observer if nothing changed.
  if (show_cleanup == cached_should_show_cleanup_in_settings_ui_) {
    return;
  }

  cached_should_show_cleanup_in_settings_ui_ = show_cleanup;
  on_show_cleanup_ui_change_.Run(show_cleanup);
}

void ChromeCleanerStateChangeObserver::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnScanning() {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnReporterRunning() {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnInfected(
    bool is_powered_by_partner,
    const ChromeCleanerScannerResults& reported_results) {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnCleaning(
    bool is_powered_by_partner,
    const ChromeCleanerScannerResults& reported_results) {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnRebootRequired() {
  OnCleanupStateChange();
}

void ChromeCleanerStateChangeObserver::OnLogsEnabledChanged(bool logs_enabled) {
  OnCleanupStateChange();
}

}  // namespace safe_browsing
