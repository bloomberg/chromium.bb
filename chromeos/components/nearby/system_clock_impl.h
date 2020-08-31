// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_SYSTEM_CLOCK_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_SYSTEM_CLOCK_IMPL_H_

#include "base/macros.h"
#include "chromeos/components/nearby/library/system_clock.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::SystemClock implementation.
class SystemClockImpl : public location::nearby::SystemClock {
 public:
  SystemClockImpl();
  ~SystemClockImpl() override;

 private:
  // location::nearby::SystemClock:
  int64_t elapsedRealtime() override;

  DISALLOW_COPY_AND_ASSIGN(SystemClockImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_SYSTEM_CLOCK_IMPL_H_
