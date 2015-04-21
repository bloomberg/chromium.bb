// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/time_source.h"

namespace content {

TimeSource::TimeSource() {
}

TimeSource::~TimeSource() {
}

base::TimeTicks TimeSource::Now() const {
  return base::TimeTicks::Now();
}

}  // namespace content
