// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidator_state.h"

#include "base/logging.h"

namespace syncer {

const char* InvalidatorStateToString(InvalidatorState state) {
  switch (state) {
    case TRANSIENT_INVALIDATION_ERROR:
      return "TRANSIENT_INVALIDATION_ERROR";
    case INVALIDATION_CREDENTIALS_REJECTED:
      return "INVALIDATION_CREDENTIALS_REJECTED";
    case INVALIDATIONS_ENABLED:
      return "INVALIDATIONS_ENABLED";
    default:
      NOTREACHED();
      return "UNKNOWN_INVALIDATOR_STATE";
  }
}

}  // namespace syncer
