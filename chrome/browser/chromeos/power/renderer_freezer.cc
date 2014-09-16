// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/renderer_freezer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

RendererFreezer::RendererFreezer(scoped_ptr<RendererFreezer::Delegate> delegate)
    : frozen_(false),
      delegate_(delegate.Pass()),
      weak_factory_(this) {
  if (delegate_->CanFreezeRenderers())
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

RendererFreezer::~RendererFreezer() {
  if (delegate_->CanFreezeRenderers())
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void RendererFreezer::SuspendImminent() {
  // If there was already a callback pending, this will cancel it and create a
  // new one.
  suspend_readiness_callback_.Reset(
      base::Bind(&RendererFreezer::OnReadyToSuspend,
                 weak_factory_.GetWeakPtr(),
                 DBusThreadManager::Get()
                     ->GetPowerManagerClient()
                     ->GetSuspendReadinessCallback()));

  base::MessageLoop::current()->PostTask(
      FROM_HERE, suspend_readiness_callback_.callback());
}

void RendererFreezer::SuspendDone(const base::TimeDelta& sleep_duration) {
  // If we get a SuspendDone before we've had a chance to run OnReadyForSuspend,
  // we should cancel it because we no longer want to freeze the renderers.  If
  // we've already run it then cancelling the callback shouldn't really make a
  // difference.
  suspend_readiness_callback_.Cancel();

  if (!frozen_)
    return;

  if (!delegate_->ThawRenderers()) {
    // We failed to write the thaw command and the renderers are still frozen.
    // We are in big trouble because none of the tabs will be responsive so
    // let's crash the browser instead.
    LOG(FATAL) << "Unable to thaw renderers.";
  }

  frozen_ = false;
}

void RendererFreezer::OnReadyToSuspend(
    const base::Closure& power_manager_callback) {
  if (delegate_->FreezeRenderers())
    frozen_ = true;

  DCHECK(!power_manager_callback.is_null());
  power_manager_callback.Run();
}

}  // namespace chromeos
