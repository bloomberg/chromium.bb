// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_OBSERVER_H_

#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Interface for objects that wish to be notified of changes in the Optimization
// Guide Service.
//
// All calls will be made on the IO thread.
class OptimizationGuideServiceObserver {
 public:
  // Called when the hints have been processed.
  virtual void OnHintsProcessed(const proto::Configuration& config) = 0;

 protected:
  virtual ~OptimizationGuideServiceObserver() {}
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_OBSERVER_H_
