// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SHUTDOWN_POLICY_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SHUTDOWN_POLICY_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {

// This class observes the device setting |DeviceRebootOnShutdown|. Changes to
// this policy are communicated to the ShutdownPolicyObserver::Delegate by
// calling its OnShutdownPolicyChanged method with the new state of the policy.
class ShutdownPolicyObserver {
 public:
  // This callback is passed to CheckIfRebootOnShutdown, which invokes it with
  // the current state of the |DeviceRebootOnShutdown| policy once a trusted of
  // policies is established.
  using RebootOnShutdownCallback = base::Callback<void(bool)>;

  // This delegate is called when the |DeviceRebootOnShutdown| policy changes.
  class Delegate {
   public:
    virtual void OnShutdownPolicyChanged(bool reboot_on_shutdown) = 0;

   protected:
    virtual ~Delegate() {}
  };

  ShutdownPolicyObserver(CrosSettings* cros_settings, Delegate* delegate);
  ~ShutdownPolicyObserver();

  // Once a trusted set of policies is established, this function calls
  // |callback| with the trusted state of the |DeviceRebootOnShutdown| policy.
  void CheckIfRebootOnShutdown(const RebootOnShutdownCallback& callback);

  // Resets the policy subscription and clears the delegate.
  void Shutdown();

 private:
  void CallDelegate(bool reboot_on_shutdown);

  void OnShutdownPolicyChanged();

  CrosSettings* cros_settings_;

  Delegate* delegate_;

  scoped_ptr<CrosSettings::ObserverSubscription> shutdown_policy_subscription_;

  base::WeakPtrFactory<ShutdownPolicyObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SHUTDOWN_POLICY_OBSERVER_H_
