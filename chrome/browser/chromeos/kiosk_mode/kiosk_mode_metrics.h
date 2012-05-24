// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_METRICS_H_
#pragma once

#include "base/time.h"

namespace chromeos {

// This class gathers metrics for Kiosk Mode.
class KioskModeMetrics {
 public:
  static KioskModeMetrics* Get();

  // This method records that the demo user session was started.
  void SessionStarted();

  // This method records that the demo user session was ended.
  void SessionEnded();

  // This method records that the user opened an app.
  void UserOpenedApp();

 private:
  KioskModeMetrics();
  ~KioskModeMetrics();

  base::Time session_started_;
  size_t apps_opened_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeMetrics);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_METRICS_H_
