// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/type_state.h"

namespace notifications {

TypeState::TypeState(SchedulerClientType type) : type(type) {}

TypeState::~TypeState() = default;

}  // namespace notifications
