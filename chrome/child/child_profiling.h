// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHILD_CHILD_PROFILING_H_
#define CHROME_CHILD_CHILD_PROFILING_H_

#include "base/macros.h"

// This class is the child-process-only (i.e. non-browser) portion of
// chrome/common/profiling.h.
class ChildProfiling {
 public:
  // Called early in a process' life to allow profiling of startup time.
  static void ProcessStarted();

 private:
  ChildProfiling();
  DISALLOW_COPY_AND_ASSIGN(ChildProfiling);
};

#endif  // CHROME_CHILD_CHILD_PROFILING_H_
