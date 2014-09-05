// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// Freezes the chrome renderers when the system is about to suspend and thaws
// them after the system fully resumes.  This class registers itself as a
// PowerManagerClient::Observer on creation and unregisters itself on
// destruction.
class CHROMEOS_EXPORT RendererFreezer : public PowerManagerClient::Observer {
 public:
  RendererFreezer();
  virtual ~RendererFreezer();

  // PowerManagerClient::Observer implementation
  virtual void SuspendImminent() OVERRIDE;
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

 private:
  void OnReadyToSuspend();

  base::FilePath state_path_;
  bool enabled_;
  bool frozen_;

  // Callback used to asynchronously report suspend readiness.
  base::Closure suspend_readiness_callback_;

  base::WeakPtrFactory<RendererFreezer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererFreezer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
