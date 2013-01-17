// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SETTINGS_H_

#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT SchedulerSettings
{
public:
  SchedulerSettings();
  ~SchedulerSettings();

  bool implSidePainting;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SETTINGS_H_
