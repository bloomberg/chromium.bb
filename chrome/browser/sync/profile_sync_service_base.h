// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_

#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "sync/internal_api/public/base/model_type.h"

namespace content {
class BrowserContext;
}

// API for ProfileSyncService.
class ProfileSyncServiceBase {
 public:
  // Retrieve the sync service to use in the given context.
  // Returns NULL if sync is not enabled for the context.
  static ProfileSyncServiceBase* FromBrowserContext(
      content::BrowserContext* context);

  typedef ProfileSyncServiceObserver Observer;

  virtual ~ProfileSyncServiceBase() {}

  // Whether sync is enabled by user or not.
  virtual bool HasSyncSetupCompleted() const = 0;

  // Returns whether processing changes is allowed.  Check this before doing
  // any model-modifying operations.
  virtual bool ShouldPushChanges() = 0;

  // Get the set of current active data types (those chosen or configured by
  // the user which have not also encountered a runtime error).
  virtual syncer::ModelTypeSet GetActiveDataTypes() const = 0;

  // Adds/removes an observer. ProfileSyncServiceBase does not take
  // ownership of the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(Observer* observer) const = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_
