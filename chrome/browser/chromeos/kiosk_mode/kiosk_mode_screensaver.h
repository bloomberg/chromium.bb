// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "ui/wm/core/user_activity_observer.h"

namespace extensions {
class Extension;
}

namespace chromeos {

class KioskModeScreensaver : public wm::UserActivityObserver {
 public:
  KioskModeScreensaver();
  virtual ~KioskModeScreensaver();

 private:
  friend class KioskModeScreensaverTest;

  // wm::UserActivityObserver overrides:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // Initialization functions, in order
  // Get the screensaver path once KioskModeHelper is initialized.
  void GetScreensaverCrxPath();

  // Callback to receive the path to the screensaver extension's crx and call
  // the unpacker to unpack and load the crx.
  void ScreensaverPathCallback(const base::FilePath& screensaver_crx);

  // Called back on the UI thread to Setup the screensaver with the now unpacked
  // and loaded extension.
  void SetupScreensaver(scoped_refptr<extensions::Extension> extension,
                        const base::FilePath& extension_base_path);

  base::FilePath extension_base_path_;

  base::WeakPtrFactory<KioskModeScreensaver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeScreensaver);
};

void InitializeKioskModeScreensaver();
void ShutdownKioskModeScreensaver();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
