// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_CLOCK_OBSERVER_H_
#define ASH_SYSTEM_DATE_CLOCK_OBSERVER_H_

namespace ash {

class ClockObserver {
 public:
  virtual ~ClockObserver() {}

  virtual void OnDateFormatChanged() = 0;
  virtual void OnSystemClockTimeUpdated() = 0;

  // Force a refresh (e.g. after the system is resumed).
  virtual void Refresh() = 0;
};

};

#endif  // ASH_SYSTEM_DATE_CLOCK_OBSERVER_H_
