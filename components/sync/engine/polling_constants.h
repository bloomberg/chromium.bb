// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants used by SyncScheduler when polling servers for updates.

#ifndef COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
#define COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_

#include <stdint.h>

#include "components/sync/base/sync_export.h"

namespace syncer {

SYNC_EXPORT extern const int64_t kDefaultShortPollIntervalSeconds;
SYNC_EXPORT extern const int64_t kDefaultLongPollIntervalSeconds;
SYNC_EXPORT extern const int64_t kMaxBackoffSeconds;
SYNC_EXPORT extern const int kBackoffRandomizationFactor;
SYNC_EXPORT extern const int kInitialBackoffRetrySeconds;
SYNC_EXPORT extern const int kInitialBackoffShortRetrySeconds;
SYNC_EXPORT extern const int kInitialBackoffImmediateRetrySeconds;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
