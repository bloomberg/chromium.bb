// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_MOCK_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_MOCK_H_
#pragma once

#include "chrome/browser/sync/syncable_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class SyncableServiceMock : public SyncableService {
 public:
  SyncableServiceMock();
  virtual ~SyncableServiceMock();

  MOCK_METHOD0(AssociateModels, bool());
  MOCK_METHOD0(DisassociateModels, bool());
  MOCK_METHOD1(SyncModelHasUserCreatedNodes, bool(bool* has_nodes));
  MOCK_METHOD0(AbortAssociation, void());
  MOCK_METHOD0(CryptoReadyIfNecessary, bool());
  MOCK_METHOD3(ApplyChangesFromSync, void(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count));
  MOCK_METHOD2(SetupSync, void(
      ProfileSyncService* sync_service,
      browser_sync::GenericChangeProcessor* change_processor));
};

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_MOCK_H_
