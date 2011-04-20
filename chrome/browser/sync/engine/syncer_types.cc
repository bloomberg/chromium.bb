// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_types.h"

namespace browser_sync {

SyncEngineEvent::SyncEngineEvent(EventCause cause) : what_happened(cause),
                                                     snapshot(NULL) {
}

SyncEngineEvent::~SyncEngineEvent() {}

}  // namespace browser_sync
