// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_INVALIDATION_FORWARDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_INVALIDATION_FORWARDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/sync_driver/sync_service_observer.h"

class ProfileSyncService;

namespace invalidation {
class P2PInvalidationService;
};

// This class links the ProfileSyncService to a P2PInvalidationService.
//
// It will observe ProfileSyncService events and emit invalidation events for
// any committed changes in observes.
//
// It register and unregisters in its constructor and destructor.  This is
// intended to make it easy to manage with a scoped_ptr.
class P2PInvalidationForwarder : public sync_driver::SyncServiceObserver {
 public:
  P2PInvalidationForwarder(
      ProfileSyncService* sync_service,
      invalidation::P2PInvalidationService* invalidation_service);
  ~P2PInvalidationForwarder() override;

  // Implementation of sync_driver::SyncServiceObserver
  void OnStateChanged() override;
  void OnSyncCycleCompleted() override;

 private:
  ProfileSyncService* sync_service_;
  invalidation::P2PInvalidationService* invalidation_service_;

  DISALLOW_COPY_AND_ASSIGN(P2PInvalidationForwarder);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_INVALIDATION_FORWARDER_H_
