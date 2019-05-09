// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_TEST_API_H_
#define ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_TEST_API_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace ash {

class TimeToFirstPresentRecorderTestApi {
 public:
  static void SetTimeToFirstPresentCallback(base::OnceClosure callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TimeToFirstPresentRecorderTestApi);
};

}  // namespace ash

#endif  // ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_TEST_API_H_
