// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/sync/api/data_type_error_handler.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/driver/change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class ChangeProcessorMock : public ChangeProcessor {
 public:
  ChangeProcessorMock();
  virtual ~ChangeProcessorMock();
  MOCK_METHOD3(ApplyChangesFromSyncModel,
               void(const syncer::BaseTransaction*,
                    int64_t,
                    const syncer::ImmutableChangeRecordList&));
  MOCK_METHOD0(CommitChangesFromSyncModel, void());
  MOCK_METHOD0(StartImpl, void());
  MOCK_CONST_METHOD0(IsRunning, bool());
  MOCK_METHOD2(OnUnrecoverableError,
               void(const tracked_objects::Location&, const std::string&));
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_
