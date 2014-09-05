// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/renderer_freezer.h"

#include <cstring>  // needed for strlen()

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {
const char kFreezerStatePath[] =
    "/sys/fs/cgroup/freezer/chrome_renderers/freezer.state";
const char kFreezeCommand[] = "FROZEN";
const char kThawCommand[] = "THAWED";

}  // namespace

void RendererFreezer::SuspendImminent() {
  suspend_readiness_callback_ = DBusThreadManager::Get()
                                    ->GetPowerManagerClient()
                                    ->GetSuspendReadinessCallback();

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RendererFreezer::OnReadyToSuspend,
                 weak_factory_.GetWeakPtr()));
}

void RendererFreezer::SuspendDone(const base::TimeDelta& sleep_duration) {
  if (!frozen_)
    return;

  if (base::WriteFile(state_path_, kThawCommand, strlen(kThawCommand)) !=
      static_cast<int>(strlen(kThawCommand))) {
    // We failed to write the thaw command and the renderers are still frozen.
    // We are in big trouble because none of the tabs will be responsive so
    // let's crash the browser instead.
    PLOG(FATAL) << "Unable to thaw processes in the cgroup freezer.";
  }

  frozen_ = false;
}

void RendererFreezer::OnReadyToSuspend() {
  if (base::WriteFile(state_path_, kFreezeCommand, strlen(kFreezeCommand)) !=
      static_cast<int>(strlen(kFreezeCommand))) {
    PLOG(WARNING) << "Unable to freeze processes in the cgroup freezer.";
  } else {
    frozen_ = true;
  }

  DCHECK(!suspend_readiness_callback_.is_null());
  suspend_readiness_callback_.Run();
  suspend_readiness_callback_.Reset();
}

RendererFreezer::RendererFreezer()
    : state_path_(base::FilePath(kFreezerStatePath)),
      enabled_(base::PathExists(state_path_) &&
               base::PathIsWritable(state_path_)),
      frozen_(false),
      weak_factory_(this) {
  if (enabled_) {
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  } else {
    LOG(WARNING) << "Cgroup freezer does not exist or is not writable. "
                 << "Processes will not be frozen during suspend.";
  }
}

RendererFreezer::~RendererFreezer() {
  if (enabled_)
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

}  // namespace chromeos
