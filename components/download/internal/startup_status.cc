// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/startup_status.h"

namespace download {

StartupStatus::StartupStatus() = default;
StartupStatus::~StartupStatus() = default;

bool StartupStatus::Complete() const {
  return driver_ok.has_value() && model_ok.has_value() &&
         file_monitor_ok.has_value();
}

bool StartupStatus::Ok() const {
  DCHECK(Complete());
  return driver_ok.value() && model_ok.value() && file_monitor_ok.value();
}

}  // namespace download
