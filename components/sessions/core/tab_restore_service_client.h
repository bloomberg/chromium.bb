// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "components/sessions/session_id.h"

namespace base {
class CancelableTaskTracker;
}

namespace sessions {
struct SessionWindow;
}

namespace sessions {

struct SessionWindow;

// Callback from TabRestoreServiceClient::GetLastSession.
// The second parameter is the id of the window that was last active.
typedef base::Callback<void(ScopedVector<SessionWindow>, SessionID::id_type)>
    GetLastSessionCallback;

// A client interface that needs to be supplied to the tab restore service by
// the embedder.
class TabRestoreServiceClient {
 public:
  virtual ~TabRestoreServiceClient() {}

  // Returns whether there is a previous session to load.
  virtual bool HasLastSession() = 0;

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  virtual void GetLastSession(const GetLastSessionCallback& callback,
                              base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
