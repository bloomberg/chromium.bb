// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

namespace cc {

SchedulerSettings::SchedulerSettings()
    : impl_side_painting(false),
      timeout_and_draw_when_animation_checkerboards(true) {}

SchedulerSettings::~SchedulerSettings() {}

}  // namespace cc
