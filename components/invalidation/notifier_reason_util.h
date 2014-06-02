// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_NOTIFIER_REASON_UTIL_H_
#define COMPONENTS_INVALIDATION_NOTIFIER_REASON_UTIL_H_

#include "jingle/notifier/listener/push_client_observer.h"
#include "sync/internal_api/public/base/invalidator_state.h"

namespace syncer {

InvalidatorState FromNotifierReason(
    notifier::NotificationsDisabledReason reason);

// Should not be called when |state| == INVALIDATIONS_ENABLED.
notifier::NotificationsDisabledReason
    ToNotifierReasonForTest(InvalidatorState state);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_NOTIFIER_REASON_UTIL_H_
