// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_MOCK_H_
#define CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_MOCK_H_
#pragma once

#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "testing/gmock/include/gmock/gmock.h"

class SyncableServiceMock : public SyncableService {
 public:
  SyncableServiceMock();
  virtual ~SyncableServiceMock();

  MOCK_METHOD3(MergeDataAndStartSyncing, bool(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor));
  MOCK_METHOD1(StopSyncing, void(syncable::ModelType type));
  MOCK_CONST_METHOD1(GetAllSyncData, SyncDataList(syncable::ModelType type));
  MOCK_METHOD1(ProcessSyncChanges, void(const SyncChangeList& change_list));
};

#endif  // CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_MOCK_H_
