// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/mock_tether_host_response_recorder.h"

namespace chromeos {

namespace tether {

MockTetherHostResponseRecorder::MockTetherHostResponseRecorder()
    : TetherHostResponseRecorder(nullptr) {}

MockTetherHostResponseRecorder::~MockTetherHostResponseRecorder() = default;

}  // namespace tether

}  // namespace chromeos
