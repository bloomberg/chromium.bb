// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/time_to_first_present_recorder_test_api.h"

#include "ash/metrics/time_to_first_present_recorder.h"
#include "ash/shell.h"
#include "base/bind.h"

namespace ash {

// static
void TimeToFirstPresentRecorderTestApi::SetTimeToFirstPresentCallback(
    base::OnceClosure callback) {
  Shell::Get()->time_to_first_present_recorder()->log_callback_ =
      std::move(callback);
}

}  // namespace ash
