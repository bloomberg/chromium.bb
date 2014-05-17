// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_prefs.h"

namespace invalidation {
namespace prefs {

// An ID to uniquely identify this client to the invalidator service.
const char kInvalidatorClientId[] = "invalidator.client_id";

// Opaque state from the invalidation subsystem that is persisted via prefs.
// The value is base 64 encoded.
const char kInvalidatorInvalidationState[] = "invalidator.invalidation_state";

// List of received invalidations that have not been acted on by any clients
// yet.  Used to keep invalidation clients in sync in case of a restart.
const char kInvalidatorSavedInvalidations[] = "invalidator.saved_invalidations";

}  // namespace prefs
}  // namespace invalidation
