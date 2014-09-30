// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VSYNC_PARAMETER_OBSERVER_H_
#define CC_OUTPUT_VSYNC_PARAMETER_OBSERVER_H_

#include "base/time/time.h"

// Interface for a class which observes the parameters regarding vsync
// information.
class VSyncParameterObserver {
 public:
  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) = 0;
};

#endif  // CC_OUTPUT_VSYNC_PARAMETER_OBSERVER_H_
