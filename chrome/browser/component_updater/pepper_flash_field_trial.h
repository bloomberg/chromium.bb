// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_FIELD_TRIAL_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_FIELD_TRIAL_H_

#include "base/basictypes.h"

class PepperFlashFieldTrial {
 public:
  static bool InEnableByDefaultGroup();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PepperFlashFieldTrial);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_FIELD_TRIAL_H_
