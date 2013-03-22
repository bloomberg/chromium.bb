// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"

#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

IdleActionWarningObserver::IdleActionWarningObserver() : warning_dialog_(NULL) {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

IdleActionWarningObserver::~IdleActionWarningObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  if (warning_dialog_)
    warning_dialog_->Close();
}

void IdleActionWarningObserver::IdleActionImminent() {
  if (!warning_dialog_)
    warning_dialog_ = new IdleActionWarningDialogView;
}

void IdleActionWarningObserver::IdleActionDeferred() {
  if (warning_dialog_)
    warning_dialog_->Close();
  warning_dialog_ = NULL;
}

}  // namespace chromeos
