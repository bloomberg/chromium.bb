// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Freezes the chrome renderers.  Returns true if the operation was
    // successful.
    virtual bool FreezeRenderers() = 0;

    // Thaws the chrome renderers.  Returns true if the operation was
    // successful.
    virtual bool ThawRenderers() = 0;

    // Returns true iff the delegate is capable of freezing renderers.
    virtual bool CanFreezeRenderers() = 0;
  };

  explicit RendererFreezer(scoped_ptr<Delegate> delegate);
  virtual ~RendererFreezer();

  // PowerManagerClient::Observer implementation
  virtual void SuspendImminent() OVERRIDE;
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

 private:
  // Called when all asynchronous work is complete and renderers can be frozen.
  void OnReadyToSuspend(const base::Closure& power_manager_callback);

  // Used to ensure that renderers do not get frozen if the suspend is canceled.
  base::CancelableClosure suspend_readiness_callback_;

  bool frozen_;

  scoped_ptr<Delegate> delegate_;

  base::WeakPtrFactory<RendererFreezer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererFreezer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
