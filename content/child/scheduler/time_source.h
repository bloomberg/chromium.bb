// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_TIME_SOURCE_H_
#define CONTENT_CHILD_SCHEDULER_TIME_SOURCE_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT TimeSource {
 public:
  TimeSource();
  virtual ~TimeSource();

  virtual base::TimeTicks Now() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeSource);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_TIME_SOURCE_H_
