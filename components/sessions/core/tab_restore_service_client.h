// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "components/sessions/session_id.h"

namespace base {
class CancelableTaskTracker;
class SequencedWorkerPool;
}

namespace sessions {
struct SessionWindow;
}

class GURL;

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

  // Returns whether a given URL should be tracked for restoring.
  virtual bool ShouldTrackURLForRestore(const GURL& url) = 0;

  // Get the sequenced worker pool for running tasks on the backend thread as
  // long as the system is not shutting down.
  virtual base::SequencedWorkerPool* GetBlockingPool() = 0;

  // Returns the path of the directory to save state into.
  virtual base::FilePath GetPathToSaveTo() = 0;

  // Returns the URL that corresponds to the new tab page.
  virtual GURL GetNewTabURL() = 0;

  // Returns whether there is a previous session to load.
  virtual bool HasLastSession() = 0;

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  virtual void GetLastSession(const GetLastSessionCallback& callback,
                              base::CancelableTaskTracker* tracker) = 0;

  // Called when a tab is restored. |url| is the URL that the tab is currently
  // visiting.
  virtual void OnTabRestored(const GURL& url) {}
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
