// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"

#include "athena/activity/public/activity_manager.h"

namespace athena {

Activity::~Activity() {
  ActivityManager::Get()->RemoveActivity(this);
}

}  // namespace athena
