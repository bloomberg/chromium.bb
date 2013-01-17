// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_RESUME_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_RESUME_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// A class to observe system resume events and dispatch onWokeUp extension API
// events.
class ResumeObserver : public PowerManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ResumeObserver();
  virtual ~ResumeObserver();

  // PowerManagerClient::Observer overrides:
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResumeObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_RESUME_OBSERVER_H_
