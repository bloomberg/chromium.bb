// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_

namespace metrics {

// Parameters to pass back to the metrics provider.
struct CallStackProfileParams {
  // The event that triggered the profile collection.
  enum Trigger {
    UNKNOWN,
    PROCESS_STARTUP,
    JANKY_TASK,
    THREAD_HUNG,
    TRIGGER_LAST = THREAD_HUNG
  };

  // Allows the caller to specify whether sample ordering is
  // important. MAY_SHUFFLE should always be used to enable better compression,
  // unless the use case needs order to be preserved for a specific reason.
  enum SampleOrderingSpec {
    // The provider may shuffle the sample order to improve compression.
    MAY_SHUFFLE,
    // The provider will not change the sample order.
    PRESERVE_ORDER
  };

  // The default constructor is required for mojo and should not be used
  // otherwise. A valid trigger should always be specified.
  CallStackProfileParams();
  explicit CallStackProfileParams(Trigger trigger);
  CallStackProfileParams(Trigger trigger, SampleOrderingSpec ordering_spec);

  // The triggering event.
  Trigger trigger;

  // Whether to preserve sample ordering.
  SampleOrderingSpec ordering_spec;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_
