// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
#pragma once

#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace browser_sync {

class ChangeProcessorMock : public ChangeProcessor {
 public:
  ChangeProcessorMock();
  virtual ~ChangeProcessorMock();
  MOCK_METHOD3(ApplyChangesFromSyncModel,
               void(const sync_api::BaseTransaction* trans,
                    const sync_api::SyncManager::ChangeRecord* changes,
                    int change_count));
  MOCK_METHOD1(StartImpl, void(Profile* profile));
  MOCK_METHOD0(StopImpl, void());
  MOCK_CONST_METHOD0(IsRunning, bool());
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
