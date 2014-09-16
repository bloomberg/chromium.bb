// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_LIGHT_BAR_H_
#define CHROME_BROWSER_CHROMEOS_POWER_LIGHT_BAR_H_

#include "base/files/file_path.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// Flashes the light bar (on systems that have one) whenever the system comes up
// in dark resume.
class CHROMEOS_EXPORT LightBar : public PowerManagerClient::Observer {
 public:
  LightBar();
  virtual ~LightBar();

  // PowerManagerClient::Observer implementation.
  virtual void DarkSuspendImminent() OVERRIDE;

 private:
  // Path to control the light bar.
  base::FilePath control_path_;

  // True if the system has a light bar.
  bool has_light_bar_;

  DISALLOW_COPY_AND_ASSIGN(LightBar);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_LIGHT_BAR_H_
