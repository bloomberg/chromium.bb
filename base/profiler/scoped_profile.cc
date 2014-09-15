// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/scoped_profile.h"

#include "base/location.h"
#include "base/tracked_objects.h"


namespace tracked_objects {


ScopedProfile::ScopedProfile(const Location& location)
    : birth_(ThreadData::TallyABirthIfActive(location)) {
  ThreadData::PrepareForStartOfRun(birth_);
}

ScopedProfile::~ScopedProfile() {
  StopClockAndTally();
}

void ScopedProfile::StopClockAndTally() {
  stopwatch_.Stop();

  if (!birth_)
    return;
  ThreadData::TallyRunInAScopedRegionIfTracking(birth_, stopwatch_);
  birth_ = NULL;
}

}  // namespace tracked_objects
