// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_TIME_SOURCE_H_
#define COMPONENTS_SCHEDULER_CHILD_TIME_SOURCE_H_

#include "base/time/time.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT TimeSource {
 public:
  TimeSource();
  virtual ~TimeSource();

  virtual base::TimeTicks Now() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeSource);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_TIME_SOURCE_H_
