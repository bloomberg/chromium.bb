// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Extension;
class Profile;

namespace chromeos {

class KioskModeScreensaver : public PowerManagerClient::Observer,
                             public content::NotificationObserver {
 public:
  KioskModeScreensaver();
  virtual ~KioskModeScreensaver();

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void ActiveNotify() OVERRIDE;

 private:
  friend class KioskModeScreensaverTest;

  // Initialization functions, in order
  // Get the screensaver path once KioskModeHelper is initialized.
  void GetScreensaverCrxPath();

  // Callback to receive the path to the screensaver extension's crx and call
  // the unpacker to unpack and load the crx.
  void ScreensaverPathCallback(const FilePath& screensaver_crx);

  // Called back on the UI thread to Setup the screensaver with the now unpacked
  // and loaded extension.
  void SetupScreensaver(scoped_refptr<Extension> extension,
                        Profile* default_profile,
                        const FilePath& extension_base_path);

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<KioskModeScreensaver> weak_ptr_factory_;

  FilePath extension_base_path_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeScreensaver);
};

void InitializeKioskModeScreensaver();
void ShutdownKioskModeScreensaver();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
