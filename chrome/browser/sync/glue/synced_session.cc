// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session.h"

#include "base/stl_util.h"
#include "chrome/browser/sessions/session_types.h"

namespace browser_sync {

SyncedSession::SyncedSession() : session_tag("invalid") {
}

SyncedSession::~SyncedSession() {
  STLDeleteElements(&windows);
}

}  // namespace browser_sync
