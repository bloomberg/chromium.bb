// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_DATE_FORMAT_OBSERVER_H_
#define ASH_SYSTEM_POWER_DATE_FORMAT_OBSERVER_H_

namespace ash {

class DateFormatObserver {
 public:
  virtual ~DateFormatObserver() {}

  virtual void OnDateFormatChanged() = 0;
};

};

#endif  // ASH_SYSTEM_POWER_DATE_FORMAT_OBSERVER_H_
