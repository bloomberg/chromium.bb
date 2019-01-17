// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_CLOCK_H_
#define API_PUBLIC_CLOCK_H_

#include "platform/api/time.h"

namespace openscreen {

class Clock {
 public:
  virtual ~Clock() = default;

  virtual platform::TimeDelta Now() = 0;
};

class PlatformClock final : public Clock {
 public:
  ~PlatformClock() override = default;

  platform::TimeDelta Now() override;
};

}  // namespace openscreen

#endif  // API_PUBLIC_CLOCK_H_
