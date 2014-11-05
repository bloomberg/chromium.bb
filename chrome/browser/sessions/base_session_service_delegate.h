// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_H_

#include "base/memory/scoped_ptr.h"

namespace base {
class SequencedWorkerPool;
}

class GURL;

// The BaseSessionServiceDelegate decouples the BaseSessionService from
// chrome/content dependencies.
class BaseSessionServiceDelegate {
 public:
  BaseSessionServiceDelegate() {}

  // Get the sequenced worker pool for running tasks on the backend thread as
  // long as the system is not shutting down.
  virtual base::SequencedWorkerPool* GetBlockingPool() = 0;

  // Tests if a given URL should be tracked.
  virtual bool ShouldTrackEntry(const GURL& url) = 0;

  // Returns true if save operations can be performed as a delayed task - which
  // is usually only used by unit tests.
  virtual bool ShouldUseDelayedSave() = 0;

  // Called when commands are about to be written to disc.
  virtual void OnWillSaveCommands() = 0;

  // Called when commands were saved to disc.
  virtual void OnSavedCommands() = 0;

 protected:
  virtual ~BaseSessionServiceDelegate() {}
};

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_H_
