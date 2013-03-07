// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_OUTPUT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_OUTPUT_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// This observer listens for when video outputs have been turned off so that the
// corresponding CRTCs can be disabled to force the connected output off.
// TODO(derat): Remove this class after powerd is calling the method
// exported by DisplayPowerServiceProvider instead.
class OutputObserver : public PowerManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  OutputObserver();
  virtual ~OutputObserver();

 private:
  // PowerManagerClient::Observer implementation.
  virtual void ScreenPowerSet(bool power_on, bool all_displays) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(OutputObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_OUTPUT_OBSERVER_H_
