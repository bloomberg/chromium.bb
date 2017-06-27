// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_

// Observer of events during session restore.
class SessionRestoreObserver {
 public:
  // OnSessionRestoreStartedLoadingTabs is triggered when the browser starts
  // loading tabs in session restore.
  virtual void OnSessionRestoreStartedLoadingTabs() {}

  // OnSessionRestoreFinishedLoadingTabs is triggered when the browser finishes
  // loading tabs in session restore. In case of memory pressure, not all
  // WebContents may have been created (i.e., some are deferred).
  virtual void OnSessionRestoreFinishedLoadingTabs() {}
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_
