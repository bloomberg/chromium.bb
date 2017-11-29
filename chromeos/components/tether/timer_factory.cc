// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/timer_factory.h"

namespace chromeos {

namespace tether {

TimerFactory::~TimerFactory() = default;

std::unique_ptr<base::Timer> TimerFactory::CreateOneShotTimer() {
  return base::MakeUnique<base::OneShotTimer>();
}

}  // namespace tether

}  // namespace chromeos
