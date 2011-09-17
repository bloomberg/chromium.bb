// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class ChangeProcessorMock
    : public ChangeProcessor, public UnrecoverableErrorHandler {
 public:
  ChangeProcessorMock();
  virtual ~ChangeProcessorMock();
  MOCK_METHOD2(ApplyChangesFromSyncModel,
               void(const sync_api::BaseTransaction*,
                    const sync_api::ImmutableChangeRecordList&));
  MOCK_METHOD0(CommitChangesFromSyncModel, void());
  MOCK_METHOD1(StartImpl, void(Profile*));
  MOCK_METHOD0(StopImpl, void());
  MOCK_CONST_METHOD0(IsRunning, bool());
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
