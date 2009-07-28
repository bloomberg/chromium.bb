// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"

///////////////////////////////////////////////////////////////////////////////
// ShellIntegration::DefaultBrowserWorker
//

ShellIntegration::DefaultBrowserWorker::DefaultBrowserWorker(
    DefaultBrowserObserver* observer)
    : observer_(observer),
      ui_loop_(MessageLoop::current()),
      file_loop_(g_browser_process->file_thread()->message_loop()) {
}

void ShellIntegration::DefaultBrowserWorker::StartCheckDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::ExecuteCheckDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::StartSetAsDefaultBrowser() {
  observer_->SetDefaultBrowserUIState(STATE_PROCESSING);
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::ExecuteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::ObserverDestroyed() {
  // Our associated view has gone away, so we shouldn't call back to it if
  // our worker thread returns after the view is dead.
  observer_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

void ShellIntegration::DefaultBrowserWorker::ExecuteCheckDefaultBrowser() {
  DCHECK(MessageLoop::current() == file_loop_);
  bool is_default = ShellIntegration::IsDefaultBrowser();
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::CompleteCheckDefaultBrowser, is_default));
}

void ShellIntegration::DefaultBrowserWorker::CompleteCheckDefaultBrowser(
    bool is_default) {
  DCHECK(MessageLoop::current() == ui_loop_);
  UpdateUI(is_default);
}

void ShellIntegration::DefaultBrowserWorker::ExecuteSetAsDefaultBrowser() {
  DCHECK(MessageLoop::current() == file_loop_);
  ShellIntegration::SetAsDefaultBrowser();
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &DefaultBrowserWorker::CompleteSetAsDefaultBrowser));
}

void ShellIntegration::DefaultBrowserWorker::CompleteSetAsDefaultBrowser() {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (observer_) {
    // Set as default completed, check again to make sure it stuck...
    StartCheckDefaultBrowser();
  }
}

void ShellIntegration::DefaultBrowserWorker::UpdateUI(bool is_default) {
  if (observer_) {
    DefaultBrowserUIState state =
        is_default ? STATE_DEFAULT : STATE_NOT_DEFAULT;
    observer_->SetDefaultBrowserUIState(state);
  }
}
