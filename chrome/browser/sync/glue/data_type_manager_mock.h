// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__

#include "chrome/browser/sync/glue/data_type_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class DataTypeManagerMock : public DataTypeManager {
 public:
  MOCK_METHOD1(Start, void(StartCallback* start_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(IsRegistered, bool(syncable::ModelType type));
  MOCK_METHOD1(IsEnabled, bool(syncable::ModelType type));
  MOCK_METHOD0(controllers, const DataTypeController::TypeMap&());
  MOCK_METHOD0(state, State());
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
