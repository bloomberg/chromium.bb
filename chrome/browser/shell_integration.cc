// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/chrome_thread.h"

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultBrowserWorker
//

ShellIntegration::DefaultBrowserWorker::DefaultBrowserWorker(
    DefaultBrowserObserver* observer)
    : observer_(observer) {
}

void ShellIntegration::DefaultBrowserWorker::StartCheckDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::ExecuteCheckDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::StartSetAsDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::ExecuteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::ObserverDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  observer_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

void ShellIntegration::DefaultBrowserWorker::ExecuteCheckDefaultBrowser() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  DefaultBrowserState state = ShellIntegration::IsDefaultBrowser();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::CompleteCheckDefaultBrowser, state));
}

void ShellIntegration::DefaultBrowserWorker::CompleteCheckDefaultBrowser(
    DefaultBrowserState state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  UpdateUI(state);
}

void ShellIntegration::DefaultBrowserWorker::ExecuteSetAsDefaultBrowser() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ShellIntegration::SetAsDefaultBrowser();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &DefaultBrowserWorker::CompleteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::CompleteSetAsDefaultBrowser() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (observer_) {
    // Set as default completed, check again to make sure it stuck...
    StartCheckDefaultBrowser();
  }
}

void ShellIntegration::DefaultBrowserWorker::UpdateUI(
    DefaultBrowserState state) {
  if (observer_) {
    switch (state) {
      case NOT_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_NOT_DEFAULT);
        break;
      case IS_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_IS_DEFAULT);
        break;
      case UNKNOWN_DEFAULT_BROWSER:
        observer_->SetDefaultBrowserUIState(STATE_UNKNOWN);
        break;
      default:
        break;
    }
  }
}
