// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profile_startup.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#include "chrome/browser/chromeos/sms_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

// TODO(beng): It is very fishy that any of these services need to be tied to
//             a browser profile. This dependency should be severed and this
//             init moved earlier in startup.
void ProfileStartup(Profile* profile, bool process_startup) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the incognito profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetOffTheRecordProfile() call above.
  profile->InitChromeOSPreferences();

  if (process_startup) {
    static chromeos::SmsObserver* sms_observer =
        new chromeos::SmsObserver(profile);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddNetworkManagerObserver(sms_observer);

    profile->SetupChromeOSEnterpriseExtensionObserver();
  }
}

}  // namespace chromeos
